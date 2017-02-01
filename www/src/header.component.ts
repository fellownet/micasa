import { Component } from '@angular/core';

declare var $: any;

@Component( {
	selector: 'header',
	templateUrl: 'tpl/header.html',
} )

export class HeaderComponent {

	public constructor() {
	};

	public toggleMenu(): void {
		if ( $( 'body' ).hasClass( 'sidebar-open' ) ) {
			$( 'body' ).removeClass( 'sidebar-open' );
		} else {
			$( 'body' ).addClass( 'sidebar-open' );
		}
	};
}
