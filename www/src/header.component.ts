import { Component } from '@angular/core';
import { AppService } from './app.service';

declare var $: any;

@Component( {
	selector: 'header',
	templateUrl: 'tpl/header.html',
} )

export class HeaderComponent {

	constructor( private appService: AppService ) {

	}

	toggleMenu(): void {
		if ( $( 'body' ).hasClass( 'sidebar-open' ) ) {
			$( 'body' ).removeClass( 'sidebar-open' );
		} else {
			$( 'body' ).addClass( 'sidebar-open' );
		}
	}

	setActiveSection( section: string ): void {
		this.appService.activeSection = section;
		$( 'body' ).removeClass( 'sidebar-open' );
	}

}
