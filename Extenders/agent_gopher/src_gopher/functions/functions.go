package functions

import (
	"archive/zip"
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"image/png"
	"io"
	"io/fs"
	"net"
	"os"
	"path"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/kbinani/screenshot"
)

const maxMsgSize = 0x2000000 // 32MB

/// FS

func CopyFile(src, dst string, info fs.FileInfo) error {
	source, err := os.Open(src)
	if err != nil {
		return err
	}
	defer func(source *os.File) {
		_ = source.Close()
	}(source)

	var mode os.FileMode = 0644
	if runtime.GOOS != "windows" {
		mode = info.Mode()
	}

	dest, err := os.OpenFile(dst, os.O_RDWR|os.O_CREATE|os.O_TRUNC, mode)
	if err != nil {
		return err
	}
	defer func(dest *os.File) {
		_ = dest.Close()
	}(dest)

	_, err = io.Copy(dest, source)
	return err
}

func CopyDir(srcDir, dstDir string) error {
	srcInfo, err := os.Stat(srcDir)
	if err != nil {
		return err
	}

	var mode os.FileMode = 0755
	if runtime.GOOS != "windows" {
		mode = srcInfo.Mode()
	}

	err = os.MkdirAll(dstDir, mode)
	if err != nil {
		return err
	}

	entries, err := os.ReadDir(srcDir)
	if err != nil {
		return err
	}

	for _, entry := range entries {
		srcPath := filepath.Join(srcDir, entry.Name())
		dstPath := filepath.Join(dstDir, entry.Name())

		info, err := entry.Info()
		if err != nil {
			return err
		}

		if info.IsDir() {
			err = CopyDir(srcPath, dstPath)
			if err != nil {
				return err
			}
		} else {
			err = CopyFile(srcPath, dstPath, info)
			if err != nil {
				return err
			}
		}
	}
	return nil
}

/// ZIP

func ZipBytes(data []byte, name string) ([]byte, error) {
	var buf bytes.Buffer
	zipWriter := zip.NewWriter(&buf)

	writer, err := zipWriter.Create(name)
	if err != nil {
		return nil, err
	}

	_, err = writer.Write(data)
	if err != nil {
		return nil, err
	}

	err = zipWriter.Close()
	if err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

func safeZipRelPath(zipName string) (string, error) {
	// Zip format mandates forward slashes. Normalize and block traversal.
	clean := path.Clean("/" + zipName)
	clean = strings.TrimPrefix(clean, "/")
	if clean == "." || clean == "" {
		return "", fmt.Errorf("invalid zip entry name: %q", zipName)
	}
	// Disallow any attempt to traverse upwards.
	if clean == ".." || strings.HasPrefix(clean, "../") || strings.Contains(clean, "/../") {
		return "", fmt.Errorf("invalid zip entry path: %q", zipName)
	}
	return filepath.FromSlash(clean), nil
}

func safeZipJoin(targetDir, zipName string) (string, error) {
	root, err := filepath.Abs(targetDir)
	if err != nil {
		return "", err
	}
	rel, err := safeZipRelPath(zipName)
	if err != nil {
		return "", err
	}
	dest := filepath.Join(root, rel)
	destAbs, err := filepath.Abs(dest)
	if err != nil {
		return "", err
	}
	rootWithSep := root + string(os.PathSeparator)
	if destAbs != root && !strings.HasPrefix(destAbs, rootWithSep) {
		return "", fmt.Errorf("zip entry escapes target dir: %q", zipName)
	}
	return destAbs, nil
}

func UnzipBytes(zipData []byte) (map[string][]byte, error) {
	result := make(map[string][]byte)
	reader := bytes.NewReader(zipData)

	zipReader, err := zip.NewReader(reader, int64(len(zipData)))
	if err != nil {
		return nil, err
	}

	for _, file := range zipReader.File {
		rel, err := safeZipRelPath(file.Name)
		if err != nil {
			return nil, err
		}
		rc, err := file.Open()
		if err != nil {
			return nil, err
		}

		var buf bytes.Buffer
		_, err = io.Copy(&buf, rc)
		_ = rc.Close()
		if err != nil {
			return nil, err
		}

		result[rel] = buf.Bytes()
	}

	return result, nil
}

func ZipFile(srcFilePath string) ([]byte, error) {
	buf := new(bytes.Buffer)
	zipWriter := zip.NewWriter(buf)

	fileToZip, err := os.Open(srcFilePath)
	if err != nil {
		return nil, err
	}
	defer fileToZip.Close()

	info, err := fileToZip.Stat()
	if err != nil {
		return nil, err
	}

	header, err := zip.FileInfoHeader(info)
	if err != nil {
		return nil, err
	}
	header.Name = filepath.Base(srcFilePath)
	header.Method = zip.Deflate

	writer, err := zipWriter.CreateHeader(header)
	if err != nil {
		return nil, err
	}

	_, err = io.Copy(writer, fileToZip)
	if err != nil {
		return nil, err
	}

	if err := zipWriter.Close(); err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

func UnzipFile(zipPath string, targetDir string) error {
	r, err := zip.OpenReader(zipPath)
	if err != nil {
		return err
	}
	defer r.Close()

	for _, f := range r.File {
		destPath, err := safeZipJoin(targetDir, f.Name)
		if err != nil {
			return err
		}

		// Создание директорий
		if f.FileInfo().IsDir() {
			err = os.MkdirAll(destPath, os.ModePerm)
			if err != nil {
				return err
			}
			continue
		}

		// Убедимся, что директория существует
		err = os.MkdirAll(filepath.Dir(destPath), os.ModePerm)
		if err != nil {
			return err
		}

		dstFile, err := os.Create(destPath)
		if err != nil {
			return err
		}

		srcFile, err := f.Open()
		if err != nil {
			_ = dstFile.Close()
			return err
		}

		_, err = io.Copy(dstFile, srcFile)
		_ = srcFile.Close()
		_ = dstFile.Close()
		if err != nil {
			return err
		}
	}

	return nil
}

func ZipDirectory(srcDir string) ([]byte, error) {
	buf := new(bytes.Buffer)
	zipWriter := zip.NewWriter(buf)

	err := filepath.Walk(srcDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}

		relPath, err := filepath.Rel(srcDir, path)
		if err != nil {
			return err
		}
		if info.IsDir() {
			if relPath == "." {
				return nil
			}
			relPath += "/"
			_, err = zipWriter.Create(relPath)
			return err
		}

		file, err := os.Open(path)
		if err != nil {
			return err
		}

		header, err := zip.FileInfoHeader(info)
		if err != nil {
			return err
		}
		header.Name = relPath
		header.Method = zip.Deflate

		writer, err := zipWriter.CreateHeader(header)
		if err != nil {
			return err
		}

		_, err = io.Copy(writer, file)
		_ = file.Close()
		return err
	})
	if err != nil {
		return nil, err
	}

	if err := zipWriter.Close(); err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

func UnzipDirectory(zipData []byte, targetDir string) error {
	reader := bytes.NewReader(zipData)
	zipReader, err := zip.NewReader(reader, int64(len(zipData)))
	if err != nil {
		return err
	}

	for _, f := range zipReader.File {
		destPath, err := safeZipJoin(targetDir, f.Name)
		if err != nil {
			return err
		}

		if f.FileInfo().IsDir() {
			err := os.MkdirAll(destPath, os.ModePerm)
			if err != nil {
				return err
			}
			continue
		}

		err = os.MkdirAll(filepath.Dir(destPath), os.ModePerm)
		if err != nil {
			return err
		}

		dstFile, err := os.Create(destPath)
		if err != nil {
			return err
		}

		srcFile, err := f.Open()
		if err != nil {
			_ = dstFile.Close()
			return err
		}

		_, err = io.Copy(dstFile, srcFile)
		_ = srcFile.Close()
		_ = dstFile.Close()
		if err != nil {
			return err
		}
	}

	return nil
}

/// SCREENS

func Screenshots() (map[int][]byte, error) {
	result := make(map[int][]byte)
	num := screenshot.NumActiveDisplays()
	for i := 0; i < num; i++ {
		img, err := screenshot.CaptureRect(screenshot.GetDisplayBounds(i))
		if err != nil {
			return nil, err
		}
		buf := new(bytes.Buffer)
		err = png.Encode(buf, img)
		if err != nil {
			return nil, err
		}
		result[i] = buf.Bytes()
	}
	return result, nil
}

/// NET

func ConnRead(conn net.Conn, size int) ([]byte, error) {
	if size <= 0 {
		return nil, fmt.Errorf("incorrected size: %d", size)
	}

	message := make([]byte, 0, size)
	tmpBuff := make([]byte, 1024)
	readSize := 0

	for readSize < size {
		toRead := size - readSize
		if toRead < len(tmpBuff) {
			tmpBuff = tmpBuff[:toRead]
		}

		n, err := conn.Read(tmpBuff)
		if err != nil {
			return nil, err
		}

		message = append(message, tmpBuff[:n]...)
		readSize += n
	}
	return message, nil
}

func RecvMsg(conn net.Conn) ([]byte, error) {
	bufLen, err := ConnRead(conn, 4)
	if err != nil {
		return nil, err
	}
	msgLen := binary.BigEndian.Uint32(bufLen)
	if msgLen == 0 || msgLen > maxMsgSize {
		return nil, fmt.Errorf("msg too big: %d", msgLen)
	}

	return ConnRead(conn, int(msgLen))
}

func SendMsg(conn net.Conn, data []byte) error {
	if conn == nil {
		return errors.New("conn is nil")
	}

	msgLen := make([]byte, 4)
	binary.BigEndian.PutUint32(msgLen, uint32(len(data)))
	message := append(msgLen, data...)
	_, err := conn.Write(message)
	return err
}
