import {
	Component,
	OnInit
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';
import { NgForm }  from '@angular/forms';

import {
	Link,
	LinksService
}                  from './links.service';

@Component( {
	templateUrl: 'tpl/link-edit.html'
} )

export class LinkEditComponent implements OnInit {

	public loading: boolean = false;
	public error: String;
	public link: Link;

	public hasAdvancedSettings: boolean = false;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _linksService: LinksService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.link = data_.link;
			for ( let setting of me.link.settings ) {
				if ( setting.class == 'advanced' ) {
					me.hasAdvancedSettings = true;
				}
			}
		} );
	};

	public submitLink() {
		var me = this;
		me.loading = true;
		this._linksService.putLink( me.link )
			.subscribe(
				function( link_: Link ) {
					if ( !!me._linksService.returnUrl ) {
						me._router.navigateByUrl( me._linksService.returnUrl );
						delete me._linksService.returnUrl;
					} else {
						me._router.navigate( [ '/links' ] );
					}
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	public deleteLink() {
		var me = this;
		me.loading = true;
		me._linksService.deleteLink( me.link )
			.subscribe(
				function( link_: Link ) {
					if ( !!me._linksService.returnUrl ) {
						me._router.navigateByUrl( me._linksService.returnUrl );
						delete me._linksService.returnUrl;
					} else {
						me._router.navigate( [ '/links' ] );
					}
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

}