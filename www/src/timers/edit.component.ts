import {
	Component,
	OnInit
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';
import { NgForm }  from '@angular/forms';

import {
	Timer,
	TimersService
}                  from './timers.service';
import { Device }  from '../devices/devices.service';

@Component( {
	templateUrl: 'tpl/timer-edit.html'
} )

export class TimerEditComponent implements OnInit {

	public timer: Timer;

	public title: string;

	public hasAdvancedSettings: boolean = false;

	public device?: Device;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _timersService: TimersService
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					this.timer = data_.timer;
					if ( 'device' in data_ ) {
						this.device = data_.device;
					}

					this.title = this.timer.name;

					for ( let setting of this.timer.settings ) {
						if ( setting.class == 'advanced' ) {
							this.hasAdvancedSettings = true;
							break;
						}
					}
				}
			)
		;
	};

	public submitTimer() {
		this._timersService.putTimer( this.timer, ( !! this.device ) ? this.device.id : undefined )
			.subscribe(
				timer_ => {
					if ( !! this._timersService.returnUrl ) {
						this._router.navigateByUrl( this._timersService.returnUrl );
						delete this._timersService.returnUrl;
					} else {
						this._router.navigate( [ '/timers' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public deleteTimer() {
		this._timersService.deleteTimer( this.timer, ( !! this.device ) ? this.device.id : undefined )
			.subscribe(
				timer_ => {
					if ( !! this._timersService.returnUrl ) {
						this._router.navigateByUrl( this._timersService.returnUrl );
						delete this._timersService.returnUrl;
					} else {
						this._router.navigate( [ '/timers' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

}
