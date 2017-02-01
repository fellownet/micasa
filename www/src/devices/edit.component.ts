import {
	Component,
	OnInit
}                            from '@angular/core';
import {
	Router,
	ActivatedRoute
}                            from '@angular/router';
import { NgForm }            from '@angular/forms';

import {
	Device,
	DevicesService
}                            from './devices.service';
import { Hardware }          from '../hardware/hardware.service';
import { Script }            from '../scripts/scripts.service';
import {
	Timer,
	TimersService
}                            from '../timers/timers.service';
import {
	Screen,
	Widget,
	ScreensService
}                            from '../screens/screens.service';

@Component( {
	templateUrl: 'tpl/device-edit.html'
} )

export class DeviceEditComponent implements OnInit {

	public loading: boolean = false;
	public error: String;
	public device: Device;
	public scripts: Script[];
	public screens: Screen[];

	public hardware?: Hardware;
	public script?: Script;

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
		this.getScreens();
		this._route.data
			.subscribe( function( data_: any ) {
				if ( 'hardware' in data_ ) {
					me.hardware = data_.hardware;
				}
				if ( 'script' in data_ ) {
					me.script = data_.script;
				}
				me.device = data_.device;
				me.scripts = data_.scripts;
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
		this._devicesService.putDevice( me.device )
			.subscribe(
				function( device_: Device ) {
					me.loading = false;
					if ( me.hardware ) {
						if ( me.hardware.parent ) {
							me._router.navigate( [ '/hardware', me.hardware.parent.id, me.hardware.id, 'edit' ] );
						} else {
							me._router.navigate( [ '/hardware', me.hardware.id, 'edit' ] );
						}
					} else if ( me.script ) {
						me._router.navigate( [ '/scripts', me.script.id ] );
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
				function( success_: boolean ) {
					me.loading = false;
					if ( success_ ) {
						if ( me.hardware ) {
							if ( me.hardware.parent ) {
								me._router.navigate( [ '/hardware', me.hardware.parent.id, me.hardware.id, 'edit' ] );
							} else {
								me._router.navigate( [ '/hardware', me.hardware.id, 'edit' ] );
							}
						} else if ( me.script ) {
							me._router.navigate( [ '/scripts', me.script.id ] );
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

	public updateSelectedScripts( id_: number, event_: any ) {
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