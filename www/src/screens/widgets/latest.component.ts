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

	@Input( 'widget' ) public widget: Widget;
	@Input( 'devices' ) private _devices: Observable<Device[]>;
	@Input( 'parent' ) public parent: WidgetComponent;
	@Input( 'editable' ) public editable: boolean;

	@Output() onAction = new EventEmitter<string>();

	private _active: boolean = true;

	public devices: Observable<Device[]>; // used in the device dropdown while editing
	public device: Device;

	public constructor(
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		var me = this;

		me.devices = me._devicesService.getDevices();

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
		me._devices.subscribe( function( devices_: Device[] ) {
			if ( devices_.length > 0 ) {
				me.device = devices_[0];
			}
		} );
	};

	public ngOnDestroy() {
		this._active = false;
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
