package main

import (
	"bytes"
	"crypto/rc4"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"regexp"
	"strings"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (m *ModuleExtender) HandlerListenerValid(data string) error {

	/// START CODE HERE

	var (
		err  error
		conf SMBConfig
	)

	err = json.Unmarshal([]byte(data), &conf)
	if err != nil {
		return err
	}

	matched, err := regexp.MatchString(`^[a-zA-Z0-9\-_.]+$`, conf.Pipename)
	if err != nil || !matched {
		return errors.New("Pipename invalid")
	}

	keyLen := len(conf.EncryptKey)
	if keyLen < 6 || keyLen > 32 {
		return errors.New("encrypt_key must be 6-32 characters")
	}

	/// END CODE

	return nil
}

func (m *ModuleExtender) HandlerCreateListenerDataAndStart(name string, configData string, listenerCustomData []byte) (adaptix.ListenerData, []byte, any, error) {
	var (
		listenerData adaptix.ListenerData
		customdData  []byte
	)

	/// START CODE HERE

	var (
		listener *SMB
		conf     SMBConfig
		err      error
	)

	if listenerCustomData == nil {
		err = json.Unmarshal([]byte(configData), &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}

		conf.Protocol = "bind_smb"

		// Normalize EncryptKey to 32 hex (16 bytes)
		keyLen := len(conf.EncryptKey)
		if keyLen == 32 {
			// Check if it's valid hex (case-insensitive)
			match, _ := regexp.MatchString("^[0-9a-fA-F]{32}$", strings.ToLower(conf.EncryptKey))
			if !match {
				// Not valid hex, hash the input string
				hash := sha256.Sum256([]byte(conf.EncryptKey))
				conf.EncryptKey = hex.EncodeToString(hash[:16])
			} else {
				// Valid hex, normalize to lowercase for consistency
				conf.EncryptKey = strings.ToLower(conf.EncryptKey)
			}
		} else {
			// Not 32 chars, hash the input string
			hash := sha256.Sum256([]byte(conf.EncryptKey))
			conf.EncryptKey = hex.EncodeToString(hash[:16])
		}
	} else {
		err = json.Unmarshal(listenerCustomData, &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}
	}

	listener = &SMB{
		Name:   name,
		Config: conf,
		Active: true,
	}

	listenerData = adaptix.ListenerData{
		BindHost:  "",
		BindPort:  "",
		AgentAddr: "\\\\.\\pipe\\" + listener.Config.Pipename,
		Status:    "Listen",
	}

	var buffer bytes.Buffer
	err = json.NewEncoder(&buffer).Encode(listener.Config)
	if err != nil {
		return listenerData, customdData, listener, err
	}
	customdData = buffer.Bytes()

	/// END CODE

	return listenerData, customdData, listener, nil
}

func (m *ModuleExtender) HandlerEditListenerData(name string, listenerObject any, configData string) (adaptix.ListenerData, []byte, bool) {
	var (
		listenerData adaptix.ListenerData
		customdData  []byte
		ok           bool = false
	)

	/// START CODE HERE

	var (
		err  error
		conf SMBConfig
	)

	listener := listenerObject.(*SMB)
	if listener.Name == name {

		err = json.Unmarshal([]byte(configData), &conf)
		if err != nil {
			return listenerData, customdData, false
		}

		listenerData = adaptix.ListenerData{
			BindHost:  "",
			BindPort:  "",
			AgentAddr: "\\\\.\\pipe\\" + listener.Config.Pipename,
			Status:    "Listen",
		}

		var buffer bytes.Buffer
		err = json.NewEncoder(&buffer).Encode(listener.Config)
		if err != nil {
			return listenerData, customdData, false
		}
		customdData = buffer.Bytes()

		ok = true
	}

	/// END CODE

	return listenerData, customdData, ok
}

func (m *ModuleExtender) HandlerListenerStop(name string, listenerObject any) (bool, error) {
	var (
		err error = nil
		ok  bool  = false
	)

	/// START CODE HERE

	listener := listenerObject.(*SMB)
	if listener.Name == name {
		ok = true
	}

	/// END CODE

	return ok, err
}

func (m *ModuleExtender) HandlerListenerGetProfile(name string, listenerObject any) ([]byte, bool) {
	var (
		object bytes.Buffer
		ok     bool = false
	)

	/// START CODE HERE

	listener := listenerObject.(*SMB)
	if listener.Name == name {
		_ = json.NewEncoder(&object).Encode(listener.Config)
		ok = true
	}

	/// END CODE

	return object.Bytes(), ok
}

func (m *ModuleExtender) HandlerListenerInteralHandler(name string, data []byte, listenerObject any) (string, error, bool) {
	var (
		agentType string
		agentId   string
		agentInfo []byte
		err       error = nil
		ok        bool  = false
	)

	/// START CODE HERE

	listener := listenerObject.(*SMB)
	if listener.Name == name {

		encKey, err := hex.DecodeString(listener.Config.EncryptKey)
		if err != nil {
			return "", err, true
		}
		rc4crypt, errcrypt := rc4.NewCipher(encKey)
		if errcrypt != nil {
			return "", errcrypt, true
		}

		agentInfo = make([]byte, len(data))
		rc4crypt.XORKeyStream(agentInfo, data)

		agentType = fmt.Sprintf("%08x", uint(binary.BigEndian.Uint32(agentInfo[:4])))
		agentInfo = agentInfo[4:]
		agentId = fmt.Sprintf("%08x", uint(binary.BigEndian.Uint32(agentInfo[:4])))
		agentInfo = agentInfo[4:]

		if !ModuleObject.ts.TsAgentIsExists(agentId) {
			_, err = ModuleObject.ts.TsAgentCreate(agentType, agentId, agentInfo, listener.Name, "", false)
			if err != nil {
				return agentId, err, false
			}
		}

		ok = true
	}

	/// END CODE

	return agentId, err, ok
}
