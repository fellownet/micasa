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
import { Script }  from '../scripts/scripts.service';

@Component( {
	templateUrl: 'tpl/timer-edit.html'
} )

export class TimerEditComponent implements OnInit {

	public timer: Timer;

	public title: string;

	public scripts?: Script[];
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
					if ( 'device' in data_ ) {
						this.device = data_.device;
					}
					if ( 'scripts' in data_ ) {
						this.scripts = data_.scripts;
					}
					this.timer = data_.timer;

					this.title = this.timer.name;
				}
			)
		;
	};

	public submitTimer() {
		if ( this.device ) {
			this.timer.device_id = this.device.id;
			delete this.timer.scripts;
		}
		this._timersService.putTimer( this.timer )
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
		this._timersService.deleteTimer( this.timer )
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

	public addScript( script_id_: number ) {
		if ( !! script_id_ ) {
			this.timer.scripts.push( +script_id_ );
		}
	};

	public removeScript( script_id_: number ) {
		let pos: number = this.timer.scripts.indexOf( script_id_ );
		if ( pos >= 0 ) {
			this.timer.scripts.splice( pos, 1 );
		}
	};

}
