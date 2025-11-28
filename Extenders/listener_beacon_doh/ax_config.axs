/// Beacon DoH listener

function ListenerUI(mode_create)
{
    // Bind settings for authoritative DNS behind public DoH
    let labelHost = form.create_label("Host & port (Bind):");
    let comboHostBind = form.create_combo();
    comboHostBind.setEnabled(mode_create);
    comboHostBind.clear();
    let addrs = ax.interfaces();
    for (let item of addrs) { comboHostBind.addItem(item); }
    let spinPortBind = form.create_spin();
    spinPortBind.setRange(1, 65535);
    spinPortBind.setValue(53);
    spinPortBind.setEnabled(mode_create);

    let labelURI = form.create_label("DoH Endpoint URI:");
    let textURI = form.create_textline("/dns-query");

    // Authoritative DNS domains (comma-separated for failover/migration)
    let labelDomain = form.create_label("Authoritative Domains:");
    let textDomain = form.create_textline("ns1.c2domain.com,ns2.backup.com");
    textDomain.setPlaceholder("Comma-separated: ns1.c2.com,ns2.backup.com");

    // Upstream DoH providers
    let labelDoHUrls = form.create_label("DoH Provider URLs (comma separated):");
    let textDoHUrls = form.create_textline("https://dns.google/dns-query,https://cloudflare-dns.com/dns-query");

    let labelUserAgent = form.create_label("User-Agent:");
    let textUserAgent = form.create_textline("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/90.0.4430.212 Safari/537.36");

    let labelPktSize = form.create_label("Max DNS payload per response (bytes):");
    let spinPktSize = form.create_spin();
    spinPktSize.setRange(512, 65535);
    spinPktSize.setValue(4096);

    let labelTTL = form.create_label("DNS TTL (seconds):");
    let spinTTL = form.create_spin();
    spinTTL.setRange(1, 3600);
    spinTTL.setValue(5);

    let labelEncryptKey = form.create_label("Encryption key (hex, 16 bytes):");
    let textEncryptKey = form.create_textline(ax.random_string(32, "hex"));
    textEncryptKey.setEnabled(mode_create);
    let buttonEncryptKey = form.create_button("Generate");
    buttonEncryptKey.setEnabled(mode_create);
    form.connect(buttonEncryptKey, "clicked", function() { textEncryptKey.setText(ax.random_string(32, "hex")); });

    // Note: SSL removed - not used in authoritative DNS mode
    // Agent uses public DoH (HTTPS) which provides transport encryption

    let layout = form.create_gridlayout();
    layout.addWidget(labelHost,        0, 0, 1, 1);
    layout.addWidget(comboHostBind,    0, 1, 1, 1);
    layout.addWidget(spinPortBind,     0, 2, 1, 1);

    layout.addWidget(labelURI,         1, 0, 1, 1);
    layout.addWidget(textURI,          1, 1, 1, 2);

    layout.addWidget(labelDomain,      2, 0, 1, 1);
    layout.addWidget(textDomain,       2, 1, 1, 2);

    layout.addWidget(labelDoHUrls,     3, 0, 1, 1);
    layout.addWidget(textDoHUrls,      3, 1, 1, 2);

    layout.addWidget(labelUserAgent,   4, 0, 1, 1);
    layout.addWidget(textUserAgent,    4, 1, 1, 2);

    layout.addWidget(labelPktSize,     5, 0, 1, 1);
    layout.addWidget(spinPktSize,      5, 1, 1, 2);

    layout.addWidget(labelTTL,         6, 0, 1, 1);
    layout.addWidget(spinTTL,          6, 1, 1, 2);

    layout.addWidget(labelEncryptKey,  7, 0, 1, 1);
    layout.addWidget(textEncryptKey,   7, 1, 1, 1);
    layout.addWidget(buttonEncryptKey, 7, 2, 1, 1);

    let container = form.create_container();
    container.put("host_bind",   comboHostBind);
    container.put("port_bind",   spinPortBind);
    container.put("uri",         textURI);
    container.put("domain",      textDomain);
    container.put("doh_urls",    textDoHUrls);
    container.put("user_agent",  textUserAgent);
    container.put("pkt_size",    spinPktSize);
    container.put("ttl",         spinTTL);
    container.put("encrypt_key", textEncryptKey);
    // "mode" is fixed to "authoritative" in GetConfig, no UI control here

    let panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container
    }
}

function GetConfig()
{
    var config = {
        "name": "Beacon DNSDoH",
        "type": "listener",
        "author": "Adaptix",
        "version": "1.0",
        "description": "Combined DNS/DoH listener designed to sit behind public DoH/recursive resolvers. Encapsulates DNS beacon traffic in HTTPS (RFC 8484) to bypass filtering.",
        "options": [
            {
                "name": "host_bind",
                "description": "Local interface to bind for DNS (UDP/TCP) used by public DoH/recursive resolvers",
                "type": "string",
                "default": "0.0.0.0",
                "required": true
            },
            {
                "name": "port_bind",
                "description": "Authoritative DNS port (53 recommended when used behind public DoH)",
                "type": "int",
                "default": "53",
                "required": true
            },
            {
                "name": "uri",
                "description": "DoH Endpoint URI",
                "type": "string",
                "default": "/dns-query",
                "required": true
            },
            {
                "name": "domain",
                "description": "Authoritative Domains (comma-separated for failover, e.g. ns1.c2.com,ns2.backup.com)",
                "type": "string",
                "default": "",
                "required": true,
                "long": true
            },
            {
                "name": "doh_urls",
                "description": "DoH Provider URLs (Comma separated)",
                "type": "string",
                "default": "https://dns.google/dns-query,https://cloudflare-dns.com/dns-query",
                "required": true,
                "long": true
            },
            {
                "name": "user_agent",
                "description": "User-Agent for DoH Requests",
                "type": "string",
                "default": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/90.0.4430.212 Safari/537.36",
                "required": true
            },
            {
                "name": "pkt_size",
                "description": "Max DNS Payload Size (bytes). 4096+ recommended for DoH.",
                "type": "int",
                "default": "4096",
                "required": true
            },
            {
                "name": "ttl",
                "description": "DNS TTL (Seconds)",
                "type": "int",
                "default": "5",
                "required": true
            },
            {
                "name": "encrypt_key",
                "description": "RC4 Encryption Key for payload encryption and agent verification (16 bytes hex)",
                "type": "string",
                "default": "random_hex_32",
                "required": true,
                "hidden": false
            },
             {
                "name": "mode",
                "description": "Listener Mode (authoritative only for now)",
                "type": "select",
                "default": "authoritative",
                "values": ["authoritative"],
                "required": true
            }
        ]
    };
    return config;
}
