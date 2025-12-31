package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strconv"
	"strings"

	adaptix "github.com/Adaptix-Framework/axc2"
)

// DoHConfig defines server-side configuration for the DoH listener.
type DoHConfig struct {
	HostBind   string   `json:"host_bind"`
	PortBind   int      `json:"port_bind"`
	Mode       string   `json:"mode"`     // authoritative | direct
	Domain     string   `json:"domain"`   // legacy single domain (for backward compat)
	Domains    []string `json:"-"`        // parsed list of domains
	DoHUrls    string   `json:"doh_urls"` // Comma separated URLs
	UserAgent  string   `json:"user_agent"`
	PktSize    int      `json:"pkt_size"`
	LabelSize  int      `json:"label_size"`
	TTL        int      `json:"ttl"`
	EncryptKey string   `json:"encrypt_key"`
	Protocol   string   `json:"protocol"`

	// HTTPS Config
	Ssl         bool   `json:"ssl"`
	SslCert     []byte `json:"ssl_cert"`
	SslKey      []byte `json:"ssl_key"`
	SslCertPath string `json:"ssl_cert_path"`
	SslKeyPath  string `json:"ssl_key_path"`
	Uri         string `json:"uri"`
}

func (m *ModuleExtender) HandlerListenerValid(data string) error {
	var conf DoHConfig
	if err := json.Unmarshal([]byte(data), &conf); err != nil {
		return err
	}

	if conf.HostBind == "" {
		return errors.New("host_bind is required")
	}
	if conf.PortBind < 1 || conf.PortBind > 65535 {
		return errors.New("port_bind must be 1-65535")
	}

	if conf.Domain == "" {
		return errors.New("domain is required")
	}
	// doh_urls is now agent-side only, no validation needed here

	keyLen := len(conf.EncryptKey)
	if keyLen < 6 || keyLen > 32 {
		return errors.New("encrypt_key must be 6-32 characters")
	}

	return nil
}

func (m *ModuleExtender) HandlerCreateListenerDataAndStart(name string, configData string, listenerCustomData []byte) (adaptix.ListenerData, []byte, any, error) {
	var (
		listenerData adaptix.ListenerData
		customData   []byte
		listener     *DoHListener
		conf         DoHConfig
		err          error
	)

	if listenerCustomData == nil {
		if err = json.Unmarshal([]byte(configData), &conf); err != nil {
			return listenerData, customData, listener, err
		}

		// Normalize EncryptKey to 32 hex (16 bytes)
		keyLen := len(conf.EncryptKey)
		if keyLen == 32 {
			if ok, _ := regexpMatchHex32(conf.EncryptKey); !ok {
				hash := sha256.Sum256([]byte(conf.EncryptKey))
				conf.EncryptKey = hex.EncodeToString(hash[:16])
			}
		} else {
			hash := sha256.Sum256([]byte(conf.EncryptKey))
			conf.EncryptKey = hex.EncodeToString(hash[:16])
		}
	} else {
		if err = json.Unmarshal(listenerCustomData, &conf); err != nil {
			return listenerData, customData, listener, err
		}
	}

	if conf.Protocol == "" {
		conf.Protocol = "doh"
	}

	// Parse comma-separated domains into slice for multi-domain support
	if conf.Domain != "" {
		for _, d := range strings.Split(conf.Domain, ",") {
			d = strings.TrimSpace(d)
			d = strings.TrimSuffix(strings.ToLower(d), ".")
			if d != "" {
				conf.Domains = append(conf.Domains, d)
			}
		}
	}

	// Defaults
	if conf.UserAgent == "" {
		conf.UserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/90.0.4430.212 Safari/537.36"
	}

	listener = &DoHListener{Config: conf, Name: name}
	if err = listener.Start(m.ts); err != nil {
		return listenerData, customData, listener, err
	}

	listenerData = adaptix.ListenerData{
		BindHost:  listener.Config.HostBind,
		BindPort:  strconv.Itoa(listener.Config.PortBind),
		AgentAddr: fmt.Sprintf("%s:%d", listener.Config.HostBind, listener.Config.PortBind),
		Protocol:  "doh",
		Status:    "Listen",
	}
	if !listener.Active {
		listenerData.Status = "Closed"
	}

	var buffer bytes.Buffer
	_ = json.NewEncoder(&buffer).Encode(listener.Config)
	customData = buffer.Bytes()

	return listenerData, customData, listener, nil
}

func (m *ModuleExtender) HandlerEditListenerData(name string, listenerObject any, configData string) (adaptix.ListenerData, []byte, bool) {
	var (
		listenerData adaptix.ListenerData
		customData   []byte
		ok           bool
	)

	listener := listenerObject.(*DoHListener)
	if listener.Name != name {
		return listenerData, customData, false
	}

	var conf DoHConfig
	if err := json.Unmarshal([]byte(configData), &conf); err != nil {
		return listenerData, customData, false
	}

	if conf.Domain != "" {
		listener.Config.Domain = conf.Domain
	}
	if conf.DoHUrls != "" {
		listener.Config.DoHUrls = conf.DoHUrls
	}
	if conf.UserAgent != "" {
		listener.Config.UserAgent = conf.UserAgent
	}
	if conf.TTL != 0 {
		listener.Config.TTL = conf.TTL
	}
	if conf.PktSize != 0 {
		listener.Config.PktSize = conf.PktSize
	}

	listenerData = adaptix.ListenerData{
		BindHost:  listener.Config.HostBind,
		BindPort:  strconv.Itoa(listener.Config.PortBind),
		AgentAddr: fmt.Sprintf("%s:%d", listener.Config.HostBind, listener.Config.PortBind),
		Protocol:  "doh",
		Status:    "Listen",
	}
	if !listener.Active {
		listenerData.Status = "Closed"
	}

	var buffer bytes.Buffer
	_ = json.NewEncoder(&buffer).Encode(listener.Config)
	customData = buffer.Bytes()
	ok = true

	return listenerData, customData, ok
}

func (m *ModuleExtender) HandlerListenerStop(name string, listenerObject any) (bool, error) {
	listener := listenerObject.(*DoHListener)
	if listener.Name != name {
		return false, nil
	}
	return true, listener.Stop()
}

func (m *ModuleExtender) HandlerListenerGetProfile(name string, listenerObject any) ([]byte, bool) {
	listener := listenerObject.(*DoHListener)
	if listener.Name != name {
		return nil, false
	}

	if listener.Config.Protocol == "" {
		listener.Config.Protocol = "doh"
	}

	var buffer bytes.Buffer
	_ = json.NewEncoder(&buffer).Encode(listener.Config)
	return buffer.Bytes(), true
}

func regexpMatchHex32(s string) (bool, error) {
	if len(s) != 32 {
		return false, nil
	}
	for i := 0; i < 32; i++ {
		c := s[i]
		if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f') {
			return false, nil
		}
	}
	return true, nil
}
