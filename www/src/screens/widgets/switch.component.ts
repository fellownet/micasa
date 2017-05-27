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
	selector: 'switchwidget',
	templateUrl: 'tpl/widgets/switch.html'
} )

export class WidgetSwitchComponent implements OnInit, OnChanges, OnDestroy {

	@Input( 'widget' ) public widget: Widget;
	@Input( 'devices' ) private _devices: Observable<Device[]>;
	@Input( 'parent' ) public parent: WidgetComponent;
	
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

		me.devices = me._devicesService.getDevices()
			.map( function( devices_: Device[] ) {
				return devices_.filter( device_ => device_.type == 'switch' );
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
		me._devices.subscribe( function( devices_: Device[] ) {
			if ( devices_.length > 0 ) {
				me.device = devices_[0];
			}
		} );
	};

	public ngOnDestroy() {
		this._active = false;
	};

	public toggle() {
		var me = this;
		if ( me.device ) {
			if ( me.device.value == 'On' ) {
				me.device.value = 'Off';
			} else {
				me.device.value = 'On';
			}
			me._devicesService.patchDevice( me.device, me.device.value )
				.subscribe(
					function( device_: Device ) {
					},
					function( error_: string ) {
					}
				)
			;
		}
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
