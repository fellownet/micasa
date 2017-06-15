import {
	Component,
	Input,
	OnInit
}                         from '@angular/core';
import { Router }         from '@angular/router';

import {
	Screen,
	ScreensService,
	Widget,
	SourceData
}                         from './screens.service';

@Component( {
	selector: 'widget',
	templateUrl: 'tpl/widget.html'
} )

export class WidgetComponent implements OnInit {

	public editing: boolean = false;
	public data: SourceData[];

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'data' ) public _data: SourceData[];
	@Input( 'devices' ) public devices: { id: number, name: string, type: string }[];

	public constructor(
		private _router: Router,
		private _screensService: ScreensService
	) {
	};

	public ngOnInit() {
		// The input data property does not update when, for instance, a widget is removed, causing the wrong data to
		// be used for the widgets that are shifted in the widgets array.
		this.data = this._data;
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

	public get self(): WidgetComponent {
		return this;
	};

	public onAction( action_: string ): void {
		switch( action_ ) {
			case 'save':
				this._screensService.putScreen( this.screen )
					.catch( error_ => this._router.navigate( [ '/error' ] ) )
					.subscribe()
				;
				break;
			case 'delete':
				let index = this.screen.widgets.indexOf( this.widget );
				if ( index > -1 ) {
					this.screen.widgets.splice( index, 1 );
					this._screensService.putScreen( this.screen )
						.catch( error_ => this._router.navigate( [ '/error' ] ) )
						.subscribe()
					;
				}
				break;
			case 'reload':
				this._screensService.getDataForWidget( this.widget ).subscribe( data_ => this.data = data_ );
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
