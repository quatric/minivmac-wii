/*
	models.h

	The Macs in the all-in-one Mini vMac Wii U app, in menu order:
	MODEL(id, name, ROM file). Each id has a configuration in cfg/<id>/, an
	EMU_OBJS_<id> list in the Makefile and a folder of its own, <id>/, in
	the app folder for its ROM and disk images.
*/

MODEL(128K,    "Macintosh 128K",  "Mac128K.ROM")
MODEL(512Ke,   "Macintosh 512Ke", "vMac.ROM")
MODEL(Plus,    "Macintosh Plus",  "vMac.ROM")
MODEL(SE,      "Macintosh SE",    "MacSE.ROM")
MODEL(Classic, "Macintosh Classic", "Classic.ROM")
MODEL(II,      "Macintosh II",    "MacII.ROM")
MODEL(IIx,     "Macintosh IIx",   "MacIIx.ROM")
