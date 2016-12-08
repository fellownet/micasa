if ( event.device.id == 1 ) {
	if ( event.value == "On" ) {
		updateDevice( 1, "Off", "after 5 seconds" );
	} else {
		updateDevice( 1, "On", "after 5 seconds" );
	}
}
