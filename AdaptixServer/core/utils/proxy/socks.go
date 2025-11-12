package proxy

import (
	"encoding/binary"
	"errors"
	"io"
	"net"
)

func CheckSocks4(conn net.Conn) (string, int, error) {
	var (
		buf     []byte
		err     error
		address string
		port    int
	)

	buf = make([]byte, 2)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	if buf[0] != 0x04 {
		err = errors.New("invalid version of socks proxy")
		goto RET
	}

	if buf[1] != 0x01 { // CONNECT command
		err = errors.New("invalid command code")
		goto RET
	}

	buf = make([]byte, 2) // port
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	port = int(binary.BigEndian.Uint16(buf))

	buf = make([]byte, 4) // IPv4
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	address = net.IP(buf).String()

RET:
	if err != nil {
		_, _ = conn.Write([]byte{0x00, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // failed
	} else {
		_, _ = conn.Write([]byte{0x00, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // success
	}

	return address, port, err
}

func CheckSocks5(conn net.Conn) (string, int, int, error) {
	var (
		buf     []byte
		err     error
		address string
		port    int
		command int
	)

	buf = make([]byte, 3)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	if buf[0] != 0x05 {
		err = errors.New("invalid version of socks proxy")
		goto RET
	}
	if buf[1] != 0x01 && buf[2] != 0x00 {
		err = errors.New("invalid version of socks proxy")
		goto RET
	}
	_, _ = conn.Write([]byte{0x05, 0x00}) // version 5, without auth

	buf = make([]byte, 4)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}

	command = int(buf[1])

	switch buf[3] {

	case 0x01: // IPv4
		ipBuffer := make([]byte, 4)
		_, err = io.ReadFull(conn, ipBuffer)
		if err != nil {
			goto RET
		}
		portBuf := make([]byte, 2)
		_, err := io.ReadFull(conn, portBuf)
		if err != nil {
			goto RET
		}
		port = int(binary.BigEndian.Uint16(portBuf))
		address = net.IP(ipBuffer).String()

		break

	case 0x03: // dns
		domainLen := make([]byte, 1)
		_, err = io.ReadFull(conn, domainLen)
		if err != nil {
			goto RET
		}
		domain := make([]byte, domainLen[0])
		_, err = io.ReadFull(conn, domain)
		if err != nil {
			goto RET
		}
		portBuf := make([]byte, 2)
		_, err = io.ReadFull(conn, portBuf)
		if err != nil {
			goto RET
		}
		port = int(binary.BigEndian.Uint16(portBuf))
		address = string(domain)

		break

	//case 0x04: // IPv6
	//	ipBuffer := make([]byte, 16)
	//	_, err = io.ReadFull(conn, ipBuffer)
	//	if err != nil {
	//		goto RET
	//	}
	//	portBuf := make([]byte, 2)
	//	_, err = io.ReadFull(conn, portBuf)
	//	if err != nil {
	//		goto RET
	//	}
	//	port = int(binary.BigEndian.Uint16(portBuf))
	//	address = net.IP(ipBuffer).String()
	//
	//	break

	default:
		err = errors.New("unsupported address format")
		goto RET
	}

RET:
	if err != nil {
		_, _ = conn.Write([]byte{0x05, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // failed
	} else {
		_, _ = conn.Write([]byte{0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // success
	}

	return address, port, command, err
}

// CheckSocks5WithoutResponse 解析 SOCKS5 握手但不发送响应
// 用于需要等待 Agent 确认后再发送响应的场景
func CheckSocks5WithoutResponse(conn net.Conn) (string, int, int, error) {
	var (
		buf     []byte
		err     error
		address string
		port    int
		command int
	)

	// 读取初始握手请求
	buf = make([]byte, 3)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		return "", 0, 0, err
	}
	if buf[0] != 0x05 {
		return "", 0, 0, errors.New("invalid version of socks proxy")
	}
	if buf[1] != 0x01 && buf[2] != 0x00 {
		return "", 0, 0, errors.New("invalid authentication method")
	}
	// 发送认证方法选择响应（必须发送）
	_, _ = conn.Write([]byte{0x05, 0x00}) // version 5, without auth

	// 读取连接请求
	buf = make([]byte, 4)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		return "", 0, 0, err
	}

	command = int(buf[1])

	switch buf[3] {
	case 0x01: // IPv4
		ipBuffer := make([]byte, 4)
		_, err = io.ReadFull(conn, ipBuffer)
		if err != nil {
			return "", 0, 0, err
		}
		portBuf := make([]byte, 2)
		_, err = io.ReadFull(conn, portBuf)
		if err != nil {
			return "", 0, 0, err
		}
		port = int(binary.BigEndian.Uint16(portBuf))
		address = net.IP(ipBuffer).String()

	case 0x03: // DNS
		domainLen := make([]byte, 1)
		_, err = io.ReadFull(conn, domainLen)
		if err != nil {
			return "", 0, 0, err
		}
		domain := make([]byte, domainLen[0])
		_, err = io.ReadFull(conn, domain)
		if err != nil {
			return "", 0, 0, err
		}
		portBuf := make([]byte, 2)
		_, err = io.ReadFull(conn, portBuf)
		if err != nil {
			return "", 0, 0, err
		}
		port = int(binary.BigEndian.Uint16(portBuf))
		address = string(domain)

	default:
		return "", 0, 0, errors.New("unsupported address format")
	}

	return address, port, command, nil
}

// SendSocks5Response 发送 SOCKS5 连接响应
func SendSocks5Response(conn net.Conn, success bool) error {
	var response []byte
	if success {
		// SOCKS5 成功响应: VER=0x05, REP=0x00(成功), RSV=0x00, ATYP=0x01(IPv4), BND.ADDR=0.0.0.0, BND.PORT=0
		response = []byte{0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	} else {
		// SOCKS5 失败响应: VER=0x05, REP=0x01(一般 SOCKS 服务器失败), RSV=0x00, ATYP=0x01(IPv4), BND.ADDR=0.0.0.0, BND.PORT=0
		response = []byte{0x05, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	}
	_, err := conn.Write(response)
	return err
}

func CheckSocks5Auth(conn net.Conn, username string, password string) (string, int, int, error) {
	var (
		buf          []byte
		err          error
		address      string
		port         int
		command      int
		size         int
		authRequired bool
		reqUsername  string
		reqPassword  string
	)

	buf = make([]byte, 2)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	if buf[0] != 0x05 {
		err = errors.New("invalid version of socks proxy")
		goto RET
	}

	size = int(buf[1])
	buf = make([]byte, size) // auth methods
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	authRequired = false
	for _, method := range buf {
		if method == 0x02 {
			authRequired = true
			break
		}
	}
	if !authRequired {
		_, _ = conn.Write([]byte{0x05, 0xFF}) // failed method
		err = errors.New("no supported authentication method")
		goto RET
	}
	_, _ = conn.Write([]byte{0x05, 0x02}) // success method

	buf = make([]byte, 2) // username
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	if buf[0] != 0x01 {
		err = errors.New("invalid auth request")
		goto RET
	}
	size = int(buf[1])
	buf = make([]byte, size)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	reqUsername = string(buf)

	buf = make([]byte, 1) // password
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	size = int(buf[0])
	buf = make([]byte, size)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	reqPassword = string(buf)

	if reqUsername != username || reqPassword != password {
		_, _ = conn.Write([]byte{0x01, 0x01}) // auth failed
		err = errors.New("authentication failed")
		goto RET
	}
	_, _ = conn.Write([]byte{0x01, 0x00}) // auth success

	buf = make([]byte, 4)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}

	command = int(buf[1])

	switch buf[3] {

	case 0x01: // IPv4
		ipBuffer := make([]byte, 4)
		_, err = io.ReadFull(conn, ipBuffer)
		if err != nil {
			goto RET
		}
		portBuf := make([]byte, 2)
		_, err := io.ReadFull(conn, portBuf)
		if err != nil {
			goto RET
		}
		port = int(binary.BigEndian.Uint16(portBuf))
		address = net.IP(ipBuffer).String()

		break

	case 0x03: // dns
		domainLen := make([]byte, 1)
		_, err = io.ReadFull(conn, domainLen)
		if err != nil {
			goto RET
		}
		domain := make([]byte, domainLen[0])
		_, err = io.ReadFull(conn, domain)
		if err != nil {
			goto RET
		}
		portBuf := make([]byte, 2)
		_, err = io.ReadFull(conn, portBuf)
		if err != nil {
			goto RET
		}
		port = int(binary.BigEndian.Uint16(portBuf))
		address = string(domain)

		break

		//case 0x04: // IPv6
		//	ipBuffer := make([]byte, 16)
		//	_, err = io.ReadFull(conn, ipBuffer)
		//	if err != nil {
		//		goto RET
		//	}
		//	portBuf := make([]byte, 2)
		//	_, err = io.ReadFull(conn, portBuf)
		//	if err != nil {
		//		goto RET
		//	}
		//	port = int(binary.BigEndian.Uint16(portBuf))
		//	address = net.IP(ipBuffer).String()
		//
		//	break

	default:
		err = errors.New("unsupported address format")
		goto RET
	}

RET:
	if err != nil {
		_, _ = conn.Write([]byte{0x05, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // failed
	} else {
		_, _ = conn.Write([]byte{0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // success
	}

	return address, port, command, err
}
