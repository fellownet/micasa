import { Component, OnInit }              from '@angular/core';
import { Router, ActivatedRoute, Params } from '@angular/router';
import { Screen, ScreenService }          from './screen.service';

import 'rxjs/add/operator/switchMap';

@Component( {
	selector: 'screen',
	templateUrl: 'tpl/screen.html'
} )

export class ScreenComponent implements OnInit {

	activeScreen: Screen;

	constructor( private _route: ActivatedRoute, private _screenService: ScreenService ) {
	}

	ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.activeScreen = data_.screen;
		} );
	};

}
