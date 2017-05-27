import {
	Component,
	Input,
	Output,
	OnInit,
	OnDestroy,
	OnChanges,
	AfterViewInit,
	ViewChild,
	ElementRef,
	EventEmitter
}                          from '@angular/core';
import { Observable }      from 'rxjs/Observable';
import { BehaviorSubject } from 'rxjs/BehaviorSubject';

import {
	Widget
}                          from '../screens.service';
import {
	Device,
	DevicesService
}                          from '../../devices/devices.service';
import { SessionService }  from '../../session/session.service';
import { WidgetComponent } from '../widget.component';

declare var require: any;
var Highcharts = require( 'highcharts/highcharts.src.js' );
require( 'highcharts/highcharts-more.src.js' )( Highcharts );
require( 'highcharts/modules/solid-gauge.src.js' )( Highcharts );

enum State {
	CREATED = 0,
	DATA_RECEIVED = 1,
	VIEW_READY = 2
};

@Component( {
	selector: 'gaugewidget',
	templateUrl: 'tpl/widgets/gauge.html'
} )

export class WidgetGaugeComponent implements OnInit, AfterViewInit, OnChanges, OnDestroy {

	@Input( 'widget' ) public widget: Widget;
	@Input( 'devices' ) private _devices: Observable<Device[]>;
	@Input( 'parent' ) public parent: WidgetComponent;
	
	@Output() onAction = new EventEmitter<string>();

	@ViewChild( 'chartTarget' ) private _chartTarget: ElementRef;
	private _state: BehaviorSubject<number> = new BehaviorSubject( State.CREATED );
	private _active: boolean = true;
	private _chart: any;

	private _colors: any = {
		aqua  : '#00c0ef',
		red   : '#dd4b39',
		yellow: '#f39c12',
		blue  : '#0073b7',
		green : '#00a65a',
		teal  : '#39cccc',
		olive : '#3d9970',
		orange: '#ff851b',
		purple: '#605ca8',
		gray  : '#AAAAAA'
	}

	public loading: boolean = true;
	public devices: Observable<Device[]>; // used in the device dropdown while editing
	public device: Device;
	public data: any[] = [];

	public constructor(
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		var me = this;

		me.devices = me._devicesService.getDevices()
			.map( function( devices_: Device[] ) {
				return devices_.filter( device_ => device_.type == 'level' );
			} )
		;

		// Render the chart when there's data received *and* thew view is ready. The data received state can happen
		// more than once, so the chart needs to be destroyed first.
		me._state.subscribe( function( state_: number ) {
			if ( ( state_ & ( State.DATA_RECEIVED | State.VIEW_READY ) ) == ( State.DATA_RECEIVED | State.VIEW_READY ) ) {
				if ( !!me._chart ) {
					me._chart.destroy();
				}

				let min: number = 0, max: number = me.device.value;
				for ( let point of me.data ) {
					let value: number = parseFloat( point.value );
					min = Math.min( min, value );
					max = Math.max( max, value );
				}
				max = Math.ceil( max * 1.05 );

				let animationDuration: number = 300;
				let config: any = {
					chart: {
						type: 'solidgauge',
						animation: {
							duration: animationDuration
						},
						style: {
							fontFamily: 'helvetica, arial, sans-serif'
						}
					},
					title: false,
					credits: false,
					tooltip: {
						enabled: false
					},
					pane: {
						startAngle: -90,
						endAngle: 90,
						background: false,
						center: [ '50%', '98%' ],
						size: '150%'
					},
					series: [ {
						name: me.device.name,
						data: [ me.device.value ],
						dataLabels: {
							format: '<div style="text-align:center"><span style="font-size:20px;color:black;font-weight:normal;">{y}</span><br/><span style="font-size:12px;color:gray;font-weight:normal;">' + me.device.unit + '</span></div>'
						}
					} ],
					yAxis: {
						min: min,
						max: max,
						labels: {
							distance: 30
						},
						tickPosition: 'outside',
						tickLength: 17,
						minorTickPosition: 'outside',
						stops: [
							[ 0, me._colors[me.widget.properties.color || 'aqua' ] ]
						]
					},
					plotOptions: {
						solidgauge: {
							outerRadius: '75%',
							animation: {
								duration: animationDuration
							},
							dataLabels: {
								y: 5,
								borderWidth: 0,
								useHTML: true
							}
						}
					}
				};

				// Skip a render pass before drawing the chart.
				setTimeout( function() {
					if ( !!me._chartTarget ) {
						me._chart = Highcharts.chart( me._chartTarget.nativeElement, config );
					}
				}, 1 );
			}
		} );

		// Listen for events broadcasted from the session service.
		me._sessionService.events
			.takeWhile( () => me._active )
			.subscribe( function( event_: any ) {
				console.log( 'got an event in screen component' );
				console.log( event_ );
			} )
		;
	};

	public ngOnChanges() {
		var me = this;
		me.loading = true;
		me._devices.subscribe( function( devices_: Device[] ) {
			if ( devices_.length > 0 ) {
				me.device = devices_[0];
				me._devicesService.getData( devices_[0].id, {
					group: '5min',
					range: 1,
					interval: 'day'
				} ).subscribe( function( data_: any[] ) {
					me.data = data_;
					me.loading = false;
					me._state.next( me._state.getValue() | State.DATA_RECEIVED );
				} );
			}
		} );
	};

	public ngAfterViewInit() {
		this._state.next( this._state.getValue() | State.VIEW_READY );
	};

	public ngOnDestroy() {
		this._active = false;
		this._state.complete();
	};

	public save() {
		this.onAction.emit( 'save' );
		this.parent.editing = false;
	};

	public delete() {
		this.onAction.emit( 'delete' );
	};

	public reload() {
		this.onAction.emit( 'reload' );
	};

	public changeType() {
		this.onAction.emit( 'type_change' );
	};

}
