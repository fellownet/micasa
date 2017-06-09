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
	Device
}                         from '../devices/devices.service'

@Component( {
	selector: 'widget',
	templateUrl: 'tpl/widget.html'
} )

export class WidgetComponent {

	public error?: string;

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'devices' ) public devices: Device[];

	public constructor(
		private _screensService: ScreensService
	) {
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
				this._screensService.putScreen( this.screen )
					.catch( error_ => this.error = error_ )
					.subscribe()
				;
				break;
			case 'delete':
				let index = this.screen.widgets.indexOf( this.widget );
				if ( index > -1 ) {
					this.screen.widgets.splice( index, 1 );
					this._screensService.putScreen( this.screen )
						.catch( error_ => this.error = error_ )
						.subscribe()
					;
				}
				break;
			case 'reload':
				this._screensService.getDevicesOnScreen( this.screen )
					.subscribe( devices_ => this.devices = devices_ )
				;
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
