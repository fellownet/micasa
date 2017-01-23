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

declare var Highcharts: any;

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

		// The defaults for all chart types are set here, the one place that is in common for all
		// widgets with charts.
		let animationDuration: number = 300;

		Highcharts.setOptions( {
			global: {
				timezoneOffset: new Date().getTimezoneOffset()
			},
			title: false,
			credits: false,
			rangeSelector: {
				buttons: [ {
					type: 'day',
					count: 1,
					text: '1d'
				}, {
					type: 'week',
					count: 1,
					text: '1w'
				}, {
					type: 'month',
					count: 1,
					text: '1m'
				}, {
					type: 'month',
					count: 3,
					text: '3m'
				}, {
					type: 'month',
					count: 6,
					text: '6m'
				}, {
					type: 'year',
					count: 1,
					text: '1y'
				} ],
				selected: 0,
				inputEnabled: false
			},
			navigator: {
				height: 75
			},
			scrollbar: {
				enabled: false
			},
			legend: {
				enabled: false
			},
			chart: {
				animation: {
					duration: animationDuration
				},
				zoomType: 'x',
				alignTicks: false
			},
			xAxis: {
				type: 'datetime',
				ordinal: false
			},
			yAxis: {
				labels: {
					align: 'left',
					x: 5
				},
				startOnTick: false,
				endOnTick: false
			},
			plotOptions: {
				column: {
					color: '#3c8dbc',
					animation: {
						duration: animationDuration
					},
					pointPadding: 0.1, // space between bars when multiple series are used
					groupPadding: 0 // space beween group of series bars
				},
				spline: {
					color: '#3c8dbc',
					animation: {
						duration: animationDuration
					},
					lineWidth: 2,
					states: {
						hover: {
							lineWidth: 2,
							halo: false
						}
					},
					marker: {
						enabled: false,
						states: {
							hover: {
								enabled: true,
								symbol: 'circle',
								radius: 6,
								lineWidth: 2
							}
						}
					}
				}
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

}
