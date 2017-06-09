import {
	Component,
	Input,
	Output,
	OnInit,
	OnDestroy,
	OnChanges,
	ViewChild,
	ElementRef,
	EventEmitter
}                         from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import {
	Widget
}                         from '../screens.service';
import {
	WidgetComponent
}                         from '../widget.component';
import {
	Device,
	DevicesService
}                         from '../../devices/devices.service'
import { SessionService } from '../../session/session.service';

@Component( {
	selector: 'latestwidget',
	templateUrl: 'tpl/widgets/latest.html'
} )

export class WidgetLatestComponent implements OnInit, OnChanges, OnDestroy {

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'devices' ) public sourceDevices: Device[];

	@Output() onAction = new EventEmitter<string>();

	private _active: boolean = true;

	public editing: boolean = false;
	public title: string;
	public device: Device; // the first and only source
	public devices: Device[]; // used in the device dropdown while editing

	public constructor(
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		var me = this;

		me.device = me.sourceDevices[me.widget.sources[0].device_id];
		me.title = me.widget.name;

		me._devicesService.getDevices().subscribe( devices_ => me.devices = devices_ );

		// Listen for events broadcasted from the session service.
		me._sessionService.events
			.takeWhile( () => me._active )
			.filter( event_ => !! me.device && event_.device_id == me.device.id )
			.subscribe( function( event_: any ) {
				me.device.value = event_.value;
				me.device.age = 0;
			} )
		;

		Observable.interval( 1000 )
			.takeWhile( () => me._active )
			.filter( () => !! me.device )
			.subscribe( () => me.device.age++ )
		;
	};

	public ngOnChanges() {
		var me = this;
		me.device = me.sourceDevices[me.widget.sources[0].device_id];
	};

	public ngOnDestroy() {
		this._active = false;
	};

	public save() {
		this.onAction.emit( 'save' );
		this.title = this.widget.name;
		this.editing = false;
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
