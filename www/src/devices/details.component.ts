import {
	Component,
	OnInit,
	AfterViewInit
}                         from '@angular/core';
import { ActivatedRoute } from '@angular/router';
import { Observable }     from 'rxjs/Observable';

import {
	Device,
	DataBundle,
	DevicesService
}                         from './devices.service';

declare var Highcharts: any;

@Component( {
	templateUrl: 'tpl/device-details.html'
} )

export class DeviceDetailsComponent implements OnInit, AfterViewInit {

	public loading: boolean = false;
	public error: String;
	public device: Device;
	public data?: any[] = [];

	public hardware?: any;
	public script?: any;
	public screen?: any;

	private _dayChart: any;
	private _weekChart: any;
	private _monthChart: any;
	private _yearChart: any;

	private _colors: string[] = [ '#3c8dbc', '#c0392d', '#f1c50d', '#28ae61', '#8d44ad', '#7e8c8d' ];

	public constructor(
		private _route: ActivatedRoute,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			if ( 'hardware' in data_ ) {
				me.hardware = data_.hardware;
			}
			if ( 'script' in data_ ) {
				me.script = data_.script;
			}
			if ( 'screen' in data_ ) {
				me.screen = data_.screen;
			}
			me.device = data_.device;





			//me.device.data = data_.data;
		} );
	};

	public ngAfterViewInit() {
		var me = this;

		// For counter and level devices a chart is drawn which is created first to have it ready when data comes in.
		if (
			me.device.type == "counter"
			|| me.device.type == "level"
		) {
			me._dayChart = Highcharts.chart( 'day_chart_target', {
				legend: {
					enabled: true
				},
				yAxis: [ {}, { opposite: true, title: { text: '' } } ]
			} );
			me._weekChart = Highcharts.chart( 'week_chart_target', {
				legend: {
					enabled: true
				},
				yAxis: [ {}, { opposite: true, title: { text: '' } } ]
			} );
			me._monthChart = Highcharts.chart( 'month_chart_target', {
				legend: {
					enabled: true
				},
				yAxis: [ {}, { opposite: true, title: { text: '' } } ]
			} );
			me._yearChart = Highcharts.chart( 'year_chart_target', {
				legend: {
					enabled: true
				},
				yAxis: [ {}, { opposite: true, title: { text: '' } } ]
			} );

			// This method attaches additional data fetches to the device observable.
			var fExtendObservable = function( observable_: Observable<Device> ) : Observable<Device> {

				return observable_
					.do( function( device_: Device ) {
						device_.dataBundle = new DataBundle();
					} )
					.mergeMap( function( device_: Device ) {
						if ( device_.type == 'level' ) {
							return me._devicesService.getData( device_.id, { group: 'none', range: 1, interval: 'day' } )
								.map( function( data_: any[] ) {
									device_.dataBundle.day = data_;
									return device_;
								} )
							;
						} else {
							return me._devicesService.getData( device_.id, { group: 'hour', range: 1, interval: 'day' } )
								.map( function( data_: any[] ) {
									device_.dataBundle.day = data_;
									return device_;
								} )
							;
						}
					} )
					.mergeMap( function( device_: Device ) {
						return me._devicesService.getData( device_.id, { group: 'day', range: 7, interval: 'day' } )
							.map( function( data_: any[] ) {
								device_.dataBundle.week = data_;
								return device_;
							} )
						;
					} )
					.mergeMap( function( device_: Device ) {
						return me._devicesService.getData( device_.id, { group: 'day', range: 1, interval: 'month' } )
							.map( function( data_: any[] ) {
								device_.dataBundle.month = data_;
								return device_;
							} )
						;
					} )
					.mergeMap( function( device_: Device ) {
						return me._devicesService.getData( device_.id, { group: 'day', range: 1, interval: 'year' } )
							.map( function( data_: any[] ) {
								device_.dataBundle.year = data_;
								return device_;
							} )
						;
					} )
				;
			};

			// First an array is created with observables that fetch the data. Note that the main device is added to
			// the observable without a fetch because it's data is already available.
			let observables: Observable<Device>[] = [];
			observables.push( fExtendObservable( Observable.of( me.device ) ) );
			if ( me.device.links ) {
				for ( let deviceId of me.device.links ) {
					observables.push( fExtendObservable( me._devicesService.getDevice( deviceId ) ) );
				}
			}

			// Then all of the observables are fetched at once.
			me.loading = true;
			Observable
				.forkJoin( observables )
				.subscribe( function( devices_: Device[] ) {

					// Add day series.
					var yAxis = -1;
					var lastUnit = null;
					var colorIndex = -1;
					for ( let device of devices_ ) {
						if ( device.unit != lastUnit ) {
							lastUnit = device.unit;
							yAxis++;
							me._dayChart.yAxis[yAxis].update( {
								title: {
									text: device.subtype + ( lastUnit.length > 0 ? ' / ' + lastUnit : '' )
								}
							} , false );
						}
						if ( yAxis > 1 ) {
							break;
						}

						var data: Array<Array<any>> = [];
						for ( var i = 0; i < device.dataBundle.day.length; i++ ) {
							data.push( [ device.dataBundle.day[i].timestamp * 1000, parseFloat( device.dataBundle.day[i].value ) ] );
						}

						var serie: any = {
							data: data,
							name: device.name,
							color: me._colors[++colorIndex % me._colors.length],
							yAxis: yAxis,
							type: device.type == 'level' ? 'spline' : 'column',
							tooltip: {
								valueSuffix: ' ' + device.unit
							}
						};
						if ( device.type == 'counter' ) {
							serie.pointRange = 3600 * 1000;
							serie.zIndex = 1;
						} else {
							serie.zIndex = 2;
						}
						me._dayChart.addSeries( serie, false );
					}
					me._dayChart.redraw();

/*
					// Add week series
					yAxis = -1;
					lastUnit = null;
					colorIndex = -1;
					for ( let device of devices_ ) {
						if ( device.unit != lastUnit ) {
							lastUnit = device.unit;
							yAxis++;
						}
						if ( yAxis > 1 ) {
							break;
						}

						var data: Array<Array<any>> = [];
						for ( var i = 0; i < device.dataBundle.week.length; i++ ) {
							data.push( [ device.dataBundle.week[i].timestamp * 1000, parseFloat( device.dataBundle.week[i].value ) ] );
						}
						
						var serie: any = {
							data: data,
							name: device.name,
							color: me._colors[++colorIndex % me._colors.length],
							yAxis: yAxis,
							type: device.type == 'level' ? 'spline' : 'column',
							tooltip: {
								valueSuffix: ' ' + device.unit
							}
						};
						me._weekChart.addSeries( serie, false );
					}
					me._weekChart.redraw();

					// Add month series
					yAxis = -1;
					lastUnit = null;
					colorIndex = -1;
					for ( let device of devices_ ) {
						if ( device.unit != lastUnit ) {
							lastUnit = device.unit;
							yAxis++;
						}
						if ( yAxis > 1 ) {
							break;
						}

						var data: Array<Array<any>> = [];
						for ( var i = 0; i < device.dataBundle.month.length; i++ ) {
							data.push( [ device.dataBundle.month[i].timestamp * 1000, parseFloat( device.dataBundle.month[i].value ) ] );
						}
						
						var serie: any = {
							data: data,
							name: device.name,
							color: me._colors[++colorIndex % me._colors.length],
							yAxis: yAxis,
							type: 'spline',
							tooltip: {
								valueSuffix: ' ' + device.unit
							}
						};
						me._monthChart.addSeries( serie, false );
					}
					me._monthChart.redraw();

					// Add year series
					yAxis = -1;
					lastUnit = null;
					colorIndex = -1;
					for ( let device of devices_ ) {
						if ( device.unit != lastUnit ) {
							lastUnit = device.unit;
							yAxis++;
						}
						if ( yAxis > 1 ) {
							break;
						}

						var data: Array<Array<any>> = [];
						for ( var i = 0; i < device.dataBundle.year.length; i++ ) {
							data.push( [ device.dataBundle.year[i].timestamp * 1000, parseFloat( device.dataBundle.year[i].value ) ] );
						}
						
						var serie: any = {
							data: data,
							name: device.name,
							color: me._colors[++colorIndex % me._colors.length],
							yAxis: yAxis,
							type: 'spline',
							tooltip: {
								valueSuffix: ' ' + device.unit
							}
						};
						me._yearChart.addSeries( serie, false );
					}
					me._yearChart.redraw();
*/
					me.loading = false;
				} )
			;
			


		} else {

			// Fetch data for switch- and text devices.
			me.loading = true;
			this._devicesService.getData( me.device.id, { range: 1, interval: 'week' } )
				.subscribe( function( data_: any ) { 
					me.data = data_;
					me.loading = false;
				} )
			;
		}














/*
			// The data for the device consists of the data for the device itself and all of the linked devices. This
			// data is fetched after the view has been initialized.
			if ( me.device.links ) {
				let observables: Observable<any[]>[] = [];	
				for ( let deviceId of me.device.links ) {
					observables.push( me._devicesService.getData( deviceId, { group: 'hour', range: 1, interval: 'year' } ) );
				}
				Observable
					.forkJoin( observables )
					.subscribe( function( data_: any[][] ) {
						me.linkedData = data_;
						console.log( data_ );
					} )
				;
			}
*/



/*
		if ( me.device.type == "counter" ) {
			var data: Array<Array<any>> = [];
			for ( var i = 0; i < me.data.length; i++ ) {
				data.push( [ me.data[i].timestamp * 1000, parseFloat( me.data[i].value ) ] );
			}

			Highcharts.stockChart( 'counter_chart_target', {
				chart: {
					type: 'column'
				},
				rangeSelector: {
					buttons: [ {
						type: 'day',
						count: 1,
						text: '1d',
						dataGrouping: {
							forced: true,
							approximation: 'sum',
							units: [ [ 'hour', [ 1 ] ] ]
						}
					}, {
						type: 'week',
						count: 1,
						text: '1w',
						dataGrouping: {
							forced: true,
							approximation: 'sum',
							units: [ [ 'day', [ 1 ] ] ]
						}
					}, {
						type: 'month',
						count: 1,
						text: '1m',
						dataGrouping: {
							forced: true,
							approximation: 'sum',
							units: [ [ 'day', [ 1 ] ] ]
						}
					}, {
						type: 'month',
						count: 3,
						text: '3m',
						dataGrouping: {
							forced: true,
							approximation: 'sum',
							units: [ [ 'day', [ 1 ] ] ]
						}
					}, {
						type: 'month',
						count: 6,
						text: '6m',
						dataGrouping: {
							forced: true,
							approximation: 'sum',
							units: [ [ 'day', [ 1 ] ] ]
						}
					}, {
						type: 'year',
						count: 1,
						text: '1y',
						dataGrouping: {
							forced: true,
							approximation: 'sum',
							units: [ [ 'month', [ 1 ] ] ]
						}
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
				series: [ {
					name: me.device.name,
					data: data,
					yAxis: 0,
					tooltip: {
						valueSuffix: ' ' + me.device.unit
					}
				} ]
			} );
		} else if ( me.device.type == "level" ) {
			var data: Array<Array<any>> = [];
			for ( var i = 0; i < me.data.length; i++ ) {
				data.push( [ me.data[i].timestamp * 1000, parseFloat( me.data[i].value ) ] );
			}

			Highcharts.stockChart( 'level_chart_target', {
				chart: {
					type: 'spline'
				},
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
				series: [ {
					name: me.device.name,
					data: data,
					yAxis: 0,
					tooltip: {
						valueSuffix: ' ' + me.device.unit
					}
				} ]
			} );
		}
		*/
	};
}
