import { Component } from '@angular/core';
import { AppService } from './app.service';

// Declaring $ as any prevents typescript errors when using jQuery.
declare var $: any;

@Component( {
	selector: 'aside',
	templateUrl: 'tpl/menu.html',
} )

export class MenuComponent  {

	constructor( private appService: AppService ) {

	}

	setActiveSection( section: string ): void {
		this.appService.activeSection = section;
		$( 'body' ).removeClass( 'sidebar-open' );
	}

}
