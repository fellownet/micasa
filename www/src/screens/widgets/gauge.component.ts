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
	EventEmitter,
	NgZone
}                          from '@angular/core';
import {
	Router
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';
import { BehaviorSubject } from 'rxjs/BehaviorSubject';

import {
	Screen,
	Widget,
	SourceData
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
	CREATED          = (1 << 0),
	DATA_RECEIVED    = (1 << 1),
	VIEW_READY       = (1 << 2)
};

@Component( {
	selector: 'gaugewidget',
	templateUrl: 'tpl/widgets/gauge.html'
} )

export class WidgetGaugeComponent implements OnInit, AfterViewInit, OnChanges, OnDestroy {

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'data' ) public data: SourceData[];
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

	public invalid: boolean = false;
	public title: string;
	public devices: Observable<{ id: number, name: string, type: string }[]>;

	public constructor(
		private _router: Router,
		private _zone: NgZone,
		private _sessionService: SessionService,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		this.title = this.widget.name;
		this.devices = this._devicesService.getDevices()
			.map( devices_ => devices_.filter( device_ => device_.type == 'level' ) )
		;

		Highcharts.setOptions( {
			lang: {
				thousandsSep: ''
			}
		} );

		// Render the chart when there's data received *and* thew view is ready. The data received state can happen
		// more than once, so the chart needs to be destroyed first.
		this._state.subscribe( state_ => {
			this._zone.runOutsideAngular( () => {
				if ( ( state_ & ( State.DATA_RECEIVED | State.VIEW_READY ) ) == ( State.DATA_RECEIVED | State.VIEW_READY ) ) {
					if ( !! this._chart ) {
						this._chart.destroy();
					}

					let min: number = 0, max: number = this.data[0].device.value;
					for ( let point of this.data[0].data ) {
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
							},
							className: 'click-target',
							events: {
								click: () => {
									this.open();
								}
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
							name: this.data[0].device.name,
							data: [ this.data[0].device.value ],
							dataLabels: {
								format: '<div style="text-align:center"><span style="font-size:20px;color:black;font-weight:normal;">{y}</span><br/><span style="font-size:12px;color:gray;font-weight:normal;">' + this.data[0].device.unit + '</span></div>'
							},
							cursor: 'pointer',
							events: {
								click: () => {
									this.open();
								}
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
								[ 0, this._colors[this.widget.properties.color || 'aqua' ] ]
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
					setTimeout( () => {
						if ( !! this._chartTarget ) {
							this._chart = Highcharts.chart( this._chartTarget.nativeElement, config );
						}
					}, 1 );
				}
			} ); // end of runOutsideAngular
		} );

		// Listen for events broadcasted from the session service.
		this._sessionService.events
			.takeWhile( () => this._active )
			.filter( event_ => event_.event == 'device_update' )
			.filter( event_ =>
				! this.parent.editing
				&& !! this._chart
				&& event_.data.id == this.data[0].device.id
			)
			.subscribe( event_ => {
				let point: any = this._chart.series[0].points[0];
				point.update( event_.data.value );
			} )
		;
	};

	public ngOnChanges() {
		if (
			!! this.data[0]
			&& !! this.data[0].device
			&& !! this.data[0].data
		) {
			this.invalid = false;
			this._state.next( this._state.getValue() | State.DATA_RECEIVED );
		} else {
			if ( !! this._chart ) {
				this._chart.destroy();
				delete this._chart;
			}
			this.invalid = true;
		}
	};

	public ngAfterViewInit() {
		this._state.next( this._state.getValue() | State.VIEW_READY );
	};

	public ngOnDestroy() {
		this._active = false;
		this._state.complete();
	};

	public open() {
		this._router.navigate( [ '/devices', this.data[0].device.id, 'details' ] );
	};

	public save() {
		this.onAction.emit( 'save' );
		this.title = this.widget.name;
		this.parent.editing = false;
		this.onAction.emit( 'reload' );
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
