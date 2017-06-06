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
	Device,
	DevicesService
}                   from './devices.service';
import { Hardware } from '../hardware/hardware.service';
import { Script }   from '../scripts/scripts.service';
import {
	Screen,
	ScreensService,
	Widget
}                   from '../screens/screens.service';

@Component( {
	templateUrl: 'tpl/device-edit.html'
} )

export class DeviceEditComponent implements OnInit {

	public loading: boolean = false;
	public error: String;
	public device: Device;
	public scripts: Script[];
	public screens: Screen[];

	public hasAdvancedSettings: boolean = false;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _devicesService: DevicesService,
		private _screensService: ScreensService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data
			.subscribe( function( data_: any ) {
				me.device = data_.device;
				me.scripts = data_.scripts;
				me.screens = data_.screens;
				for ( let setting of me.device.settings ) {
					if ( setting.class == 'advanced' ) {
						me.hasAdvancedSettings = true;
					}
				}
			}
		);
	};

	public submitDevice( form_: NgForm ) {
		var me = this;
		me.loading = true;
		me._devicesService.putDevice( me.device )
			.subscribe(
				function( device_: Device ) {
					if ( !!me._devicesService.returnUrl ) {
						me._router.navigateByUrl( me._devicesService.returnUrl );
						delete me._devicesService.returnUrl;
					} else {
						me._router.navigate( [ '/devices' ] );
					}
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	public deleteDevice() {
		var me = this;
		me.loading = true;
		me._devicesService.deleteDevice( me.device )
			.subscribe(
				function( device_: Device ) {
					if ( !!me._devicesService.returnUrl ) {
						me._router.navigateByUrl( me._devicesService.returnUrl );
						delete me._devicesService.returnUrl;
					} else {
						me._router.navigate( [ '/devices' ] );
					}
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	public addScript( script_id_: number ) {
		if ( !! script_id_ ) {
			this.device.scripts.push( +script_id_ );
		}
	};

	public removeScript( script_id_: number ) {
		let pos: number = this.device.scripts.indexOf( script_id_ );
		if ( pos >= 0 ) {
			this.device.scripts.splice( pos, 1 );
		}
	};

	public addToScreen( screen_: Screen ) {
		var me = this;
		let widget: Widget;

		switch( me.device.type ) {
			case 'switch':
				widget = {
					type: 'switch',
					size: 'small',
					name: me.device.name,
					properties: {
						color: 'aqua'
					},
					sources: [ {
						device_id: me.device.id,
						properties: {
						}
					} ]
				};
				break;
			case 'text':
				widget = {
					type: 'table',
					size: 'large',
					name: me.device.name,
					properties: {
						color: 'aqua'
					},
					sources: [ {
						device_id: me.device.id,
						properties: {
						}
					} ]
				};
				break;
			case 'level':
				widget = {
					type: 'gauge',
					size: 'small',
					name: me.device.name,
					properties: {
						color: 'blue'
					},
					sources: [ {
						device_id: me.device.id,
						properties: {
							color: 'blue'
						}
					} ],
					interval: 'week',
					range: 1
				};
				break;
			case 'counter':
				widget = {
					type: 'chart',
					size: 'large',
					name: me.device.name,
					properties: {
						color: 'blue'
					},
					sources: [ {
						device_id: me.device.id,
						properties: {
							color: 'blue'
						}
					} ],
					interval: 'month',
					range: 1
				};
				break;
		}

		screen_.widgets.push( widget );
		me._screensService.putScreen( screen_ )
			.subscribe( function( screens_: Screen[] ) {
				if ( screen_.id == 1 ) {
					me._router.navigate( [ '/dashboard' ] );
				} else {
					me._router.navigate( [ '/screens', screen_.id ] );
				}
			} )
		;
	};

}
