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
	selector: 'latestwidget',
	templateUrl: 'tpl/widgets/latest.html'
} )

export class WidgetLatestComponent implements OnInit, OnChanges, OnDestroy {

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'data' ) public data: SourceData[];
	@Input( 'devices' ) public devices: Device[];
	@Input( 'parent' ) public parent: WidgetComponent;

	@Output() onAction = new EventEmitter<string>();

	private _active: boolean = true;

	public invalid: boolean = false;
	public title: string;

	public constructor(
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		var me = this;

		me.title = me.widget.name;

		// Listen for events broadcasted from the session service.
		me._sessionService.events
			.takeWhile( () => me._active )
			.filter( event_ => ! this.invalid && event_.device_id == me.data[0].device.id )
			.subscribe( function( event_: any ) {
				me.data[0].device.value = event_.value;
				me.data[0].device.age = 0;
			} )
		;

		Observable.interval( 1000 )
			.takeWhile( () => me._active )
			.filter( () => ! this.invalid )
			.subscribe( () => this.data[0].device.age++ )
		;
	};

	public ngOnChanges() {
		var me = this;
		this.invalid = (
			! this.data[0]
			|| ! this.data[0].device
		);
	};

	public ngOnDestroy() {
		this._active = false;
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
