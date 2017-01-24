import { Component, OnInit, Input }      from '@angular/core';
import { Router, ActivatedRoute } from '@angular/router';
import { Device, DevicesService } from './devices.service';
import { Hardware }               from '../hardware/hardware.service';
import { Script, ScriptsService } from '../scripts/scripts.service';
import {
	Screen,
	Widget,
	ScreensService
}                                 from '../screens/screens.service';
import { SettingsComponent }      from '../settings/settings.component';

@Component( {
	templateUrl: 'tpl/device-details.html'
} )

export class DeviceDetailsComponent implements OnInit {

	loading: boolean = false;
	error: String;
	device: Device;
	hardware?: Hardware;
	scripts: Script[] = [];
	screens: Screen[] = [];

	@Input( "settingsComponent" ) public settingsComponent: SettingsComponent;

	constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _devicesService: DevicesService,
		private _scriptsService: ScriptsService,
		private _screensService: ScreensService
	) {
	};

	ngOnInit() {
		var me = this;
		this.getScripts();
		this.getScreens();

		this._route.data.subscribe( function( data_: any ) {
			me.device = data_.device;
		} );

		// Fortunately the resolved device also contains the hardware, which we need if coming from
		// the hardware details page. The resolver also checked if the id's match.
		this._route.params.subscribe( function( params_: any ) {
			if ( params_.hardware_id ) {
				me.hardware = me.device.hardware;
			}
		} );
	};

	submitDevice() {
		var me = this;
		me.loading = true;
		this._devicesService.putDevice( me.device )
			.subscribe(
				function( device_: Device ) {
					me.loading = false;
					if ( me.hardware ) {
						me._router.navigate( [ '/hardware', me.hardware.id ] );
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

	deleteDevice() {
		var me = this;
		me.loading = true;
		me._devicesService.deleteDevice( me.device )
			.subscribe(
				function( success_: boolean ) {
					me.loading = false;
					if ( success_ ) {
						if ( me.hardware ) {
							me._router.navigate( [ '/hardware', me.hardware.id ] );
						} else {
							me._router.navigate( [ '/devices' ] );
						}
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

	getScripts() {
		var me = this;
		this._scriptsService.getScripts()
			.subscribe(
				function( scripts_: Script[]) {
					me.scripts = scripts_;
				},
				function( error_: String ) {
					me.error = error_;
				}
			)
		;
	};

	public getScreens() {
		var me = this;
		this._screensService.getScreens()
			.subscribe(
				function( screens_: Screen[] ) {
					me.screens = screens_;
				},
				function( error_: String ) {
					me.error = error_;
				}
			)
		;
	};

	updateSelectedScripts( id_: number, event_: any ) {
		let target: any = event.target;
		if ( target.checked ) {
			if ( this.device.scripts.indexOf( id_ ) < 0 ) {
				this.device.scripts.push( id_ );
			}
		} else {
			let pos: number = this.device.scripts.indexOf( id_ );
			if ( pos >= 0 ) {
				this.device.scripts.splice( pos, 1 );
			}
		}
	};

	public addToScreen( index_: number ): void {
		var me = this;
		me.loading = true;

		let screen: Screen = me.screens[index_];
		let widget: Widget = { device_id: this.device.id };
		screen.widgets.push( widget );

		me._screensService.putScreen( screen )
			.subscribe(
				function( screen_: Screen ) {
					me.loading = false;
					if ( index_ > 0 ) {
						me._router.navigate( [ '/screen', index_ ] );
					} else {
						me._router.navigate( [ '/dashboard' ] );
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
