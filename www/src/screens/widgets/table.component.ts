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

	@Input( 'widget' ) public widget: Widget;
	@Input( 'devices' ) private _devices: Observable<Device[]>;
	@Input( 'parent' ) public parent: WidgetComponent;
	@Input( 'editable' ) public editable: boolean;

	@Output() onAction = new EventEmitter<string>();

	private _active: boolean = true;

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
				return devices_.filter( device_ => device_.type == 'switch' || device_.type == 'text' );
			} )
		;

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
					range: 1,
					interval: 'week'
				} ).subscribe( function( data_: any[] ) {
					me.data = data_;
					me.loading = false;
				} );
			}
		} );
	};

	public ngOnDestroy() {
		this._active = false;
	};

	public save() {
		this.onAction.emit( 'save' );
		this.onAction.emit( 'reload' );
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
