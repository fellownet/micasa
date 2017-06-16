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
import {
	Router
}                          from '@angular/router'; 
import { Observable }      from 'rxjs/Observable';

import {
	Screen,
	SourceData,
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
	@Input( 'data' ) public data: SourceData[];
	@Input( 'parent' ) public parent: WidgetComponent;

	@Output() onAction = new EventEmitter<string>();

	private _active: boolean = true;

	public invalid: boolean = false;
	public title: string;
	public devices: Observable<{ id: number, name: string, type: string }[]>;

	public constructor(
		private _router: Router,
		private _sessionService: SessionService,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		this.title = this.widget.name;
		this.devices = this._devicesService.getDevices( { enabled: 1 } )
			.map( devices_ => devices_.filter( device_ => device_.type != 'level' && device_.type != 'counter' ) )
		;

		// Listen for events broadcasted from the session service.
		this._sessionService.events
			.takeWhile( () => this._active )
			.filter( event_ => ! this.parent.editing && ! this.invalid && event_.device_id == this.data[0].device.id )
			.subscribe( event_ => {
				this.data[0].data.push( { timestamp: Math.floor( Date.now() / 1000 ), value: event_.value } );
				this.data[0].data = this.data[0].data.slice( 0 ); // dirty change detection
			} )
		;
	};

	public ngOnChanges() {
		this.invalid = (
			! this.data[0]
			|| ! this.data[0].device
			|| ! this.data[0].data
		);
	};

	public ngOnDestroy() {
		this._active = false;
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
