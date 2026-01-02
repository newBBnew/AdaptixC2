package server

// TsGetExtensionPath returns the configured extension path from profile
func (ts *Teamserver) TsGetExtensionPath() string {
	if ts.Profile == nil || ts.Profile.Server == nil {
		return ""
	}
	return ts.Profile.Server.ExtensionPath
}
