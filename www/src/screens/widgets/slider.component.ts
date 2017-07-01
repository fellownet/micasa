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
import { Subject }        from 'rxjs/Subject';

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
	selector: 'sliderwidget',
	templateUrl: 'tpl/widgets/slider.html'
} )

export class WidgetSliderComponent implements OnInit, OnChanges, OnDestroy {

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'data' ) public data: SourceData[];
	@Input( 'parent' ) public parent: WidgetComponent;

	@Output() onAction = new EventEmitter<string>();

	@ViewChild( 'sliderHandle' ) private _handle: ElementRef;
	@ViewChild( 'sliderTrack' ) private _track: ElementRef;

	private _active: boolean = true;
	private _busy?: number;
	private _sliding: Subject<MouseEvent> = new Subject();

	public invalid: boolean = false;
	public title: string;
	public devices: Observable<{ id: number, name: string, type: string }[]>;
	public sliding: any;

	public constructor(
		private _router: Router,
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		this.title = this.widget.name;
		this.devices = this._devicesService.getDevices()
			.map( devices_ => devices_.filter( device_ => device_.type == 'level' && ! device_.readonly ) )
		;

		// Listen for events broadcasted from the session service.
		this._sessionService.events
			.takeWhile( () => this._active )
			.filter( event_ => event_.event == 'device_update' )
			.filter( event_ =>
				! this.parent.editing
				&& ! this.invalid
				&& event_.data.id == this.data[0].device.id
			)
			.subscribe( event_ => {
				this.data[0].device.value = event_.data.value;
				this.data[0].device.age = 0;
				setTimeout(
					() => delete this._busy,
					Math.max( 0, 350 - ( Date.now() - this._busy ) )
				);
			} )
		;

		this._sliding
			.takeWhile( () => this._active )
			.throttleTime( 20 )
			.subscribe( event_ => {
				if ( !! this.sliding ) {
					let diff: number = event_.clientX - this.sliding.mouse_x;
					let perc: number = ( 100 / this.sliding.track_width ) * diff;
					this.data[0].device.value = Math.max( 0, Math.min( 100, Math.round( this.sliding.value + perc ) ) );
				}
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

	public mouseDown( event_: MouseEvent ) {
		this.sliding = {
			mouse_x: event_.clientX,
			track_width: this._track.nativeElement.offsetWidth,
			value: this.data[0].device.value
		};

		event_.stopPropagation();
		event_.preventDefault();
		event_.cancelBubble = true;
		event_.returnValue = false;
		return false;
	};

	public mouseMove( event_: MouseEvent ) {
		if ( !! this.sliding ) {
			this._sliding.next( event_ );

			event_.stopPropagation();
			event_.preventDefault();
			event_.cancelBubble = true;
			event_.returnValue = false;
			return false;
		} else {
			return true;
		}
	};

	public mouseUp( event_: MouseEvent ) {
		if ( !! this.sliding ) {
			delete this.sliding;

			let now: number = Date.now();
			this._busy = now;
			setTimeout( () => {
				if ( this._busy == now ) {
					delete( this._busy );
				}
			}, 5000 );
			this._devicesService.patchDevice( this.data[0].device, this.data[0].device.value ).subscribe();
		}
	};

	public get busy() {
		return !! this._busy;
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
