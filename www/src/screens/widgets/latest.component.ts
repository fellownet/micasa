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
	WidgetComponent
}                          from '../widget.component';
import {
	Device,
	DevicesService
}                          from '../../devices/devices.service'
import { SessionService }  from '../../session/session.service';

declare var require: any;
const Highcharts = require( 'highcharts/highcharts.src.js' );

enum State {
	CREATED          = (1 << 0),
	DATA_RECEIVED    = (1 << 1),
	VIEW_READY       = (1 << 2)
};

@Component( {
	selector: 'latestwidget',
	templateUrl: 'tpl/widgets/latest.html'
} )

export class WidgetLatestComponent implements OnInit, AfterViewInit, OnChanges, OnDestroy {

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
		this.devices = this._devicesService.getDevices();

		// Render the chart when there's data received *and* thew view is ready. The data received state can happen
		// more than once, so the chart needs to be destroyed first.
		this._state.subscribe( state_ => {
			this._zone.runOutsideAngular( () => {
				if ( ( state_ & ( State.DATA_RECEIVED | State.VIEW_READY ) ) == ( State.DATA_RECEIVED | State.VIEW_READY ) ) {
					if ( !! this._chart ) {
						this._chart.destroy();
					}

					let data: any[] = [];
					for ( let point of this.data[0].data ) {
						data.push( { x: point.timestamp * 1000, y: point.value } );
					}

					let config: any = {
						chart: {
							backgroundColor: null,
							margin: this.data[0].device.type == 'level' ? [ 0, -5, 0, -5 ] : [ 0, 0, 0, 0 ],
							className: 'click-target',
							events: {
								click: () => {
									this.open();
								}
							}

						},
						tooltip: {
							enabled: false
						},
						title: false,
						credits: false,
						series: [ {
							data: data,
							color: 'rgba( 0, 0, 0, 0.2 )',
							yAxis: 0,
							type: this.data[0].device.type == 'level' ? 'spline' : 'column',
							cursor: 'pointer',
							events: {
								click: () => {
									this.open();
								}
							}
						} ],
						legend: {
							enabled: false
						},
						xAxis: {
							type: 'datetime',
							title: {
								text: null
							},
							labels: {
								enabled: false
							},
							lineWidth: 0,
							tickPosition: 'inside',
							tickWidth: 0
						},
						yAxis: [ {
							gridLineWidth: 0,
							startOnTick: false,
							endOnTick: false,
							title: {
								text: null
							},
							labels: {
								enabled: false
							}
						} ],
						plotOptions: {
							spline: {
								animation: false,
								lineWidth: 1.5,
								states: {
									hover: {
										enabled: false
									}
								},
								marker: {
									enabled: false
								}
							},
							column: {
								animation: false,
								pointPadding: 0.05,
								groupPadding: 0.1,
								borderWidth: 0
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
				! this.parent.editing &&
				!! this._chart
				&& ! this.invalid
				&& event_.data.id == this.data[0].device.id
			)
			.subscribe( event_ => {
				this.data[0].device.value = event_.data.value;
				this.data[0].device.age = 0;

				let serie: any = this._chart.series[0];
				serie.addPoint( { x: Date.now(), y: event_.data.value }, true, true, false );
			} )
		;

		Observable.interval( 1000 )
			.takeWhile( () => this._active )
			.filter( () => ! this.invalid )
			.subscribe( () => this.data[0].device.age++ )
		;
	};

	public ngOnChanges() {
		this.invalid = (
			! this.data[0]
			|| ! this.data[0].device
		);
		if (
			! this.invalid
			&& (
				this.data[0].device.type == 'level'
				|| this.data[0].device.type == 'counter'
			)
		) {
			this._state.next( this._state.getValue() | State.DATA_RECEIVED );
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
