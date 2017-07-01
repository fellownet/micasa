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
import { Device }  from '../devices/devices.service';

@Component( {
	templateUrl: 'tpl/link-edit.html'
} )

export class LinkEditComponent implements OnInit {

	public link: Link;

	public title: string;

	public showDeleteWarning: boolean = false;
	public hasAdvancedSettings: boolean = false;

	public device?: Device;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _linksService: LinksService
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					this.link = data_.link;
					if ( 'device' in data_ ) {
						this.device = data_.device;
					}

					this.title = this.link.name;

					this.showDeleteWarning = false;
					for ( let setting of this.link.settings ) {
						if ( setting.class == 'advanced' ) {
							this.hasAdvancedSettings = true;
							break;
						}
					}
				}
			)
		;
	};

	public submitLink() {
		this._linksService.putLink( this.link, ( !! this.device ) ? this.device.id : undefined )
			.subscribe(
				link_ => {
					if ( !! this._linksService.returnUrl ) {
						this._router.navigateByUrl( this._linksService.returnUrl );
						delete this._linksService.returnUrl;
					} else {
						this._router.navigate( [ '/links' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public deleteLink() {
		this._linksService.deleteLink( this.link, ( !! this.device ) ? this.device.id : undefined )
			.subscribe(
				link_ => {
					if ( !! this._linksService.returnUrl ) {
						this._router.navigateByUrl( this._linksService.returnUrl );
						delete this._linksService.returnUrl;
					} else {
						this._router.navigate( [ '/links' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

}