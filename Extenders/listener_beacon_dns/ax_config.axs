/// Beacon DNS listener

function ListenerUI(mode_create)
{
	// MAIN SETTINGS
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

	let labelDomain = form.create_label("Domain:");
	let textDomain = form.create_textline("example.com");

	let labelQType = form.create_label("Query type:");
	let comboQType = form.create_combo();
	comboQType.addItems(["TXT", "A", "AAAA"]);
	comboQType.setCurrentIndex(0);

	let labelPktSize = form.create_label("Max payload per response (bytes):");
	let spinPktSize = form.create_spin();
	spinPktSize.setRange(16, 64000);
	spinPktSize.setValue(1024);

	let labelTTL = form.create_label("TTL (seconds):");
	let spinTTL = form.create_spin();
	spinTTL.setRange(1, 3600);
	spinTTL.setValue(10);

	let labelResolvers = form.create_label("Resolvers (optional, comma-separated):");
	let textResolvers = form.create_textline("");

	let labelLabelSize = form.create_label("Max label length (Base32 chars):");
	let spinLabelSize = form.create_spin();
	spinLabelSize.setRange(8, 63);
	spinLabelSize.setValue(48);

	let labelEncryptKey = form.create_label("Encryption key (hex, 16 bytes):");
	let textEncryptKey = form.create_textline(ax.random_string(32, "hex"));
	textEncryptKey.setEnabled(mode_create);
	let buttonEncryptKey = form.create_button("Generate");
	buttonEncryptKey.setEnabled(mode_create);

	form.connect(buttonEncryptKey, "clicked", function() { textEncryptKey.setText( ax.random_string(32, "hex") ); });

	let layout = form.create_gridlayout();
	layout.addWidget(labelHost,        0, 0, 1, 1);
	layout.addWidget(comboHostBind,    0, 1, 1, 1);
	layout.addWidget(spinPortBind,     0, 2, 1, 1);
	layout.addWidget(labelDomain,      1, 0, 1, 1);
	layout.addWidget(textDomain,       1, 1, 1, 2);
	layout.addWidget(labelQType,       2, 0, 1, 1);
	layout.addWidget(comboQType,       2, 1, 1, 2);
	layout.addWidget(labelPktSize,     3, 0, 1, 1);
	layout.addWidget(spinPktSize,      3, 1, 1, 2);
	layout.addWidget(labelTTL,         4, 0, 1, 1);
	layout.addWidget(spinTTL,          4, 1, 1, 2);
	layout.addWidget(labelResolvers,   5, 0, 1, 1);
	layout.addWidget(textResolvers,    5, 1, 1, 2);
	layout.addWidget(labelLabelSize,   6, 0, 1, 1);
	layout.addWidget(spinLabelSize,    6, 1, 1, 2);
	layout.addWidget(labelEncryptKey,  7, 0, 1, 1);
	layout.addWidget(textEncryptKey,   7, 1, 1, 1);
	layout.addWidget(buttonEncryptKey, 7, 2, 1, 1);

	let container = form.create_container();
	container.put("host_bind",   comboHostBind);
	container.put("port_bind",   spinPortBind);
	container.put("domain",      textDomain);
	container.put("qtype",       comboQType);
	container.put("pkt_size",    spinPktSize);
	container.put("ttl",         spinTTL);
	container.put("encrypt_key", textEncryptKey);
	container.put("resolvers",   textResolvers);
	container.put("label_size",  spinLabelSize);

	let panel = form.create_panel();
	panel.setLayout(layout);

	return {
		ui_panel: panel,
		ui_container: container
	}
}
