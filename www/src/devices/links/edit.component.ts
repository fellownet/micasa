import {
	Component,
	OnInit
}                   from '@angular/core';
import {
	Router,
	ActivatedRoute
}                   from '@angular/router';
import { NgForm }   from '@angular/forms';

import {
	Link,
	Device,
	DevicesService
}                   from '../devices.service';
import { Hardware } from '../../hardware/hardware.service';
import { Script }   from '../../scripts/scripts.service';
import {
	Timer,
	TimersService
}                   from '../../timers/timers.service';
import { Screen }   from '../../screens/screens.service';

@Component( {
	templateUrl: 'tpl/link-edit.html'
} )

export class LinkEditComponent implements OnInit {

	public loading: boolean = false;
	public error: String;
	public link: Link;
	public device: Device;

	public hardware?: Hardware;
	public script?: Script;
	public screen?: Screen;

	public hasAdvancedSettings: boolean = false;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data
			.subscribe( function( data_: any ) {
				if ( 'hardware' in data_ ) {
					me.hardware = data_.hardware;
				}
				if ( 'script' in data_ ) {
					me.script = data_.script;
				}
				if ( 'screen' in data_ ) {
					me.screen = data_.screen;
				}
				me.link = data_.link;
				me.device = data_.device;
				for ( let setting of me.link.settings ) {
					if ( setting.class == 'advanced' ) {
						me.hasAdvancedSettings = true;
					}
				}
			}
		);
	};

	public submitLink( form_: NgForm ) {
		var me = this;
		me.loading = true;
		this._devicesService.putLink( me.device, me.link )
			.subscribe(
				function( link_: Link ) {
					me._router.navigate( [ '/devices', me.device.id, 'edit' ] );
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
		me._devicesService.deleteLink( me.device, me.link )
			.subscribe(
				function( success_: boolean ) {
					if ( success_ ) {
						me._router.navigate( [ '/devices', me.device.id, 'edit' ] );
					} else {
						this.error = 'Unable to delete device.';
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
