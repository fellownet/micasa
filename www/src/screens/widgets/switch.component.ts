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
import {
	Router
}                         from '@angular/router';
import { Observable }     from 'rxjs/Observable';

import {
	Screen,
	Widget,
	SourceData
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
	@Input( 'data' ) public data: SourceData[];
	@Input( 'parent' ) public parent: WidgetComponent;

	@Output() onAction = new EventEmitter<string>();

	private _active: boolean = true;
	private _busy?: number;

	public invalid: boolean = false;
	public title: string;
	public devices: Observable<{ id: number, name: string, type: string }[]>;

	public constructor(
		private _router: Router,
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		this.title = this.widget.name;
		this.devices = this._devicesService.getDevices()
			.map( devices_ => devices_.filter( device_ => device_.type == 'switch' ) )
		;

		// Listen for events broadcasted from the session service.
		this._sessionService.events
			.takeWhile( () => this._active )
			.filter( event_ => ! this.parent.editing && ! this.invalid && event_.device_id == this.data[0].device.id )
			.subscribe( event_ => {
				this.data[0].device.value = event_.value;
				this.data[0].device.age = 0;
				setTimeout(
					() => delete this._busy,
					Math.max( 0, 350 - ( Date.now() - this._busy ) )
				);
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
	};

	public ngOnDestroy() {
		this._active = false;
	};

	public get busy() {
		return !! this._busy;
	};

	public toggle() {
		if (
			! this.invalid
			&& ! this._busy
		) {
			let value: string;
			if ( this.data[0].device.value == 'On' ) {
				value = 'Off';
			} else {
				value = 'On';
			}
			let now: number = Date.now();
			this._busy = now;
			setTimeout( () => {
				if ( this._busy == now ) {
					delete( this._busy );
				}
			}, 5000 );
			this._devicesService.patchDevice( this.data[0].device, value ).subscribe();
		}
	};

	public open() {
		this._router.navigate( [ '/devices', this.data[0].device.id, 'details' ] );
	};

	public save() {
		this.onAction.emit( 'save' );
		this.title = this.widget.name;
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
