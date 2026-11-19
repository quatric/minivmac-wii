/*
	Partial link of one model (see the Makefile): its data and bss each end
	up in a single input section of the final link, between these symbols,
	which the build renames mv_<model>_data_start and so on. Everything
	else is kept as it is.
*/
SECTIONS
{
	.data : { data_start = .; *(.data .data.*) data_end = .; }
	.bss : { bss_start = .; *(.bss .bss.* COMMON) bss_end = .; }
}
