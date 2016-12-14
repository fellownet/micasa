import { Component, Input } from '@angular/core';

declare var $: any;

@Component( {
	selector: 'header',
	templateUrl: 'tpl/header.html',
} )

export class HeaderComponent {

	@Input() fullname: String = "TEST";

	toggleMenu(): void {
		if ( $( 'body' ).hasClass( 'sidebar-open' ) ) {
			$( 'body' ).removeClass( 'sidebar-open' ).removeClass( 'sidebar-collapse' );
		} else {
			$( 'body' ).addClass( 'sidebar-open' );
		}
	}
}
