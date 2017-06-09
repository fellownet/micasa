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

import {
	Screen,
	Widget
}                          from '../screens.service';
import {
	Device,
	DevicesService
}                          from '../../devices/devices.service';
import { SessionService }  from '../../session/session.service';
import { WidgetComponent } from '../widget.component';

@Component( {
	selector: 'tablewidget',
	templateUrl: 'tpl/widgets/table.html'
} )

export class WidgetTableComponent implements OnInit, OnChanges, OnDestroy {

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'devices' ) public sourceDevices: Device[];

	@Output() onAction = new EventEmitter<string>();

	private _active: boolean = true;

	public editing: boolean = false;
	public title: string;
	public device: Device; // the first and only source
	public devices: Device[]; // used in the device dropdown while editing
	public data: any[] = [];

	public constructor(
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		var me = this;

		me.device = me.sourceDevices[me.widget.sources[0].device_id];
		me.title = me.widget.name;

		me._devicesService.getDevices()
			.map( function( devices_: Device[] ) {
				return devices_.filter( device_ => device_.type == 'switch' || device_.type == 'text' );
			} )
			.subscribe( devices_ => me.devices = devices_ )
		;

		// Listen for events broadcasted from the session service.
		me._sessionService.events
			.takeWhile( () => me._active )
			.filter( event_ => !! me.device && event_.device_id == me.device.id )
			.subscribe( function( event_: any ) {
				me.data.push( { timestamp: Math.floor( Date.now() / 1000 ), value: event_.value } );
				me.data = me.data.slice( 0 ); // dirty change detection
			} )
		;
	};

	public ngOnChanges() {
		var me = this;
		me.device = me.sourceDevices[me.widget.sources[0].device_id];
		me._devicesService.getData( me.device.id, {
			range: 1,
			interval: 'week'
		} ).subscribe( function( data_: any[] ) {
			me.data = data_;
		} );
	};

	public ngOnDestroy() {
		this._active = false;
	};

	public save() {
		this.onAction.emit( 'save' );
		this.title = this.widget.name;
		this.editing = false;
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
