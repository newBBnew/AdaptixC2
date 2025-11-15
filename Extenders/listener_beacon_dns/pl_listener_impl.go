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

// DNSConfig defines server-side configuration for the DNS listener.
// NOTE: Protocol is included in the profile JSON so that agent_beacon
// can correctly detect the transport type ("dns") when generating
// DNS beacon payloads.
type DNSConfig struct {
	HostBind   string `json:"host_bind"`
	PortBind   int    `json:"port_bind"`
	Mode       string `json:"mode"`   // authoritative | direct
	Domain     string `json:"domain"` // example.com
	Resolvers  string `json:"resolvers"`
	QType      string `json:"qtype"`      // TXT/A/AAAA
	LabelSize  int    `json:"label_size"` // reserved for future tuning
	PktSize    int    `json:"pkt_size"`
	TTL        int    `json:"ttl"`
	EncryptKey string `json:"encrypt_key"`
	Protocol   string `json:"protocol"`
}

func (m *ModuleExtender) HandlerListenerValid(data string) error {
	var conf DNSConfig
	if err := json.Unmarshal([]byte(data), &conf); err != nil {
		return err
	}

	if conf.HostBind == "" {
		return errors.New("host_bind is required")
	}
	if conf.PortBind < 1 || conf.PortBind > 65535 {
		return errors.New("port_bind must be 1-65535")
	}

	if conf.QType != "" {
		qt := strings.ToUpper(conf.QType)
		if qt != "TXT" && qt != "A" && qt != "AAAA" {
			return errors.New("qtype must be TXT/A/AAAA")
		}
	}

	if conf.Mode == "authoritative" && conf.Domain == "" {
		return errors.New("domain is required in authoritative mode")
	}

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
		listener     *DNS
		conf         DNSConfig
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

	// Ensure protocol is always set in the stored config/profile so that
	// agent_beacon can read listenerMap["protocol"] == "dns".
	if conf.Protocol == "" {
		conf.Protocol = "dns"
	}

	listener = &DNS{Config: conf, Name: name}
	if err = listener.Start(m.ts); err != nil {
		return listenerData, customData, listener, err
	}

	listenerData = adaptix.ListenerData{
		BindHost:  listener.Config.HostBind,
		BindPort:  strconv.Itoa(listener.Config.PortBind),
		AgentAddr: fmt.Sprintf("%s:%d", listener.Config.HostBind, listener.Config.PortBind),
		Protocol:  "dns",
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

	listener := listenerObject.(*DNS)
	if listener.Name != name {
		return listenerData, customData, false
	}

	var conf DNSConfig
	if err := json.Unmarshal([]byte(configData), &conf); err != nil {
		return listenerData, customData, false
	}

	if conf.Domain != "" {
		listener.Config.Domain = conf.Domain
	}
	if conf.QType != "" {
		listener.Config.QType = strings.ToUpper(conf.QType)
	}
	if conf.TTL != 0 {
		listener.Config.TTL = conf.TTL
	}
	if conf.PktSize != 0 {
		listener.Config.PktSize = conf.PktSize
	}
	if conf.Resolvers != "" {
		listener.Config.Resolvers = conf.Resolvers
	}

	listenerData = adaptix.ListenerData{
		BindHost:  listener.Config.HostBind,
		BindPort:  strconv.Itoa(listener.Config.PortBind),
		AgentAddr: fmt.Sprintf("%s:%d", listener.Config.HostBind, listener.Config.PortBind),
		Protocol:  "dns",
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
	listener := listenerObject.(*DNS)
	if listener.Name != name {
		return false, nil
	}
	return true, listener.Stop()
}

func (m *ModuleExtender) HandlerListenerGetProfile(name string, listenerObject any) ([]byte, bool) {
	listener := listenerObject.(*DNS)
	if listener.Name != name {
		return nil, false
	}

	// Backward compatibility: existing listeners created before Protocol
	// field was added may have empty Config.Protocol. Normalize it here
	// so profiles always carry "protocol": "dns".
	if listener.Config.Protocol == "" {
		listener.Config.Protocol = "dns"
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
