import {
	Component,
	Input,
	OnInit
}                         from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import {
	Screen,
	ScreensService,
	Widget
}                         from './screens.service';
import {
	Device,
	DevicesService
}                         from '../devices/devices.service'
//import { SessionService } from '../session/session.service';

@Component( {
	selector: 'widget',
	templateUrl: 'tpl/widget.html'
} )

export class WidgetComponent implements OnInit {

	public error?: string;

	@Input( 'screen' ) public screen: Screen;
	@Input( 'device' ) public device?: Device;
	@Input( 'widget' ) public widget: Widget;

	public devices: Observable<Device[]>;
	public editing: boolean = false;

	public constructor(
		private _screensService: ScreensService,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		this.devices = this._gatherDevices();
	};

	private _gatherDevices(): Observable<Device[]> {
		var me = this;
		let observables: Observable<Device>[] = [];
		let done: number[] = [];
		for ( let source of me.widget.sources ) {
			if ( done.indexOf( source.device_id ) > -1 ) {
				continue;
			}
			if (
				!!me.device
				&& me.device.id == source.device_id
			) {
				observables.push( Observable.of( me.device ) );
			} else {
				observables.push(
					me._devicesService.getDevice( source.device_id )

						// NOTE we're capturing http errors when the device cannot be fetched. If we wouldn't, the
						// forkJoin would never complete and the widget becomes unusable. Instead, the failed source is
						// removed from the widget and null (!) is passed.
						.catch( function( error_: string ) {
							let index: number = me.widget.sources.indexOf( source );
							if ( index > -1 ) {
								me.widget.sources.splice( index, 1 );
							}
							return Observable.of( null );
						} )
				);
			}
			done.push( source.device_id );
		}

		return Observable
			.forkJoin( observables )
			.map( function( devices_: Device[] ) {
				return devices_.filter(
					device_ => device_ !== null // filter out failed devices (see NOTE above)
				);
			} )
			.do( function( devices_: Device[] ) {

				// NOTE if there are no devices in the widget the widget itself is automatically removed.
				if ( devices_.length == 0 ) {
					let index: number = me.screen.widgets.indexOf( me.widget );
					if ( index > -1 ) {
						me.screen.widgets.splice( index, 1 );
						// TODO shouldn't the screen be saved here? For instance; the dashboard cannot be saved and
						// would then keep on trying to fetch the widget devices.
					}
				}
			} )
		;
	};

	public get self(): WidgetComponent { // allows to pass the component to the child through input binding
		return this;
	};

	public get classes(): any {
		return {
			'col-xs-12': true,
			'col-sm-12': true,
			'col-md-12': this.widget.size == 'large',
			'col-md-6': this.widget.size == 'medium' || this.widget.size == 'small',
			'col-lg-12': this.widget.size == 'large',
			'col-lg-6': this.widget.size == 'medium',
			'col-lg-4': this.widget.size == 'small'
		};
	};

	public onAction( action_: string ): void {
		switch( action_ ) {
			case 'save':
				this._screensService.putScreen( this.screen ).subscribe();
				break;
			case 'delete':
				let index = this.screen.widgets.indexOf( this.widget );
				if ( index > -1 ) {
					this.screen.widgets.splice( index, 1 );
					this._screensService.putScreen( this.screen ).subscribe();
				}
				break;
			case 'reload':
				this.devices = this._gatherDevices();
				break;
			case 'type_change':
				switch( this.widget.type ) {
					case 'table':
					case 'chart':
						this.widget.size = 'large';
						break;
					case 'gauge':
						this.widget.size = 'medium';
						break;
					case 'switch':
						this.widget.size = 'small';
						break;
				}
				break;
		}
	};

}
