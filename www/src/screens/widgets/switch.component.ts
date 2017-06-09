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
	Screen,
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

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'devices' ) public sourceDevices: Device[];

	@Output() onAction = new EventEmitter<string>();

	private _active: boolean = true;
	private _busy?: number;

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

		me._devicesService.getDevices()
			.map( function( devices_: Device[] ) {
				return devices_.filter( device_ => device_.type == 'switch' );
			} )
			.subscribe( devices_ => me.devices = devices_ )
		;

		// Listen for events broadcasted from the session service.
		me._sessionService.events
			.takeWhile( () => me._active )
			.filter( event_ => !! me.device && event_.device_id == me.device.id )
			.subscribe( function( event_: any ) {
				me.device.value = event_.value;
				me.device.age = 0;
				setTimeout( function() {
					delete( me._busy );
				}, Math.max( 0, 350 - ( Date.now() - me._busy ) ) );
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

	public get busy() {
		return !! this._busy;
	};

	public toggle() {
		var me = this;
		if (
			! me._busy
			&& me.device
		) {
			let value: string;
			if ( me.device.value == 'On' ) {
				value = 'Off';
			} else {
				value = 'On';
			}
			let now: number = Date.now();
			me._busy = now;
			setTimeout( function() {
				if ( me._busy == now ) {
					delete( me._busy );
				}
			}, 5000 );
			me._devicesService.patchDevice( me.device, value ).subscribe();
		}
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
