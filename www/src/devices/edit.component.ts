import {
	Component,
	OnInit
}                         from '@angular/core';
import {
	Router,
	ActivatedRoute
}                         from '@angular/router';
import { NgForm }         from '@angular/forms';

import {
	Device,
	DevicesService
}                         from './devices.service';
import {
	Screen,
	ScreensService,
	Widget
}                         from '../screens/screens.service';

@Component( {
	templateUrl: 'tpl/device-edit.html'
} )

export class DeviceEditComponent implements OnInit {

	public device: Device;
	public screens: Screen[];

	public title: string;

	public hasAdvancedSettings: boolean = false;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _devicesService: DevicesService,
		private _screensService: ScreensService
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					this.device = data_.device;
					this.screens = data_.screens;

					this.title = this.device.name;

					for ( let setting of this.device.settings ) {
						if ( setting.class == 'advanced' ) {
							this.hasAdvancedSettings = true;
							break;
						}
					}
				}
			)
		;
	};

	public submitDevice( form_: NgForm ) {
		this._devicesService.putDevice( this.device )
			.subscribe(
				device_ => {
					if ( !! this._devicesService.returnUrl ) {
						this._router.navigateByUrl( this._devicesService.returnUrl );
						delete this._devicesService.returnUrl;
					} else {
						this._router.navigate( [ '/devices' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public deleteDevice() {
		this._devicesService.deleteDevice( this.device )
			.subscribe(
				device_ => {
					if ( !! this._devicesService.returnUrl ) {
						this._router.navigateByUrl( this._devicesService.returnUrl );
						delete this._devicesService.returnUrl;
					} else {
						this._router.navigate( [ '/devices' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public addToScreen( screen_: Screen ) {
		let widget: Widget;
		switch( this.device.type ) {
			case 'switch':
				widget = {
					type: this.device.readonly ? 'latest' : 'switch',
					size: 'small',
					name: this.device.name,
					properties: {
						color: 'aqua'
					},
					sources: [ {
						device_id: this.device.id,
						properties: {
						}
					} ]
				};
				break;
			case 'text':
				widget = {
					type: 'table',
					size: 'large',
					name: this.device.name,
					properties: {
						color: 'aqua'
					},
					sources: [ {
						device_id: this.device.id,
						properties: {
						}
					} ]
				};
				break;
			case 'level':
				widget = {
					type: 'gauge',
					size: 'small',
					name: this.device.name,
					properties: {
						color: 'blue'
					},
					sources: [ {
						device_id: this.device.id,
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
					name: this.device.name,
					properties: {
						color: 'blue'
					},
					sources: [ {
						device_id: this.device.id,
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

		this._screensService.putScreen( screen_ )
			.subscribe(
				() => {
					if ( screen_.id == 1 ) {
						this._router.navigate( [ '/dashboard' ] );
					} else {
						this._router.navigate( [ '/screens', screen_.id ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

}
