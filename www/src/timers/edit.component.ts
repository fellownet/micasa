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

	public error: String;
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
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			if ( 'device' in data_ ) {
				me.device = data_.device;
			}
			if ( 'scripts' in data_ ) {
				me.scripts = data_.scripts;
			}
			me.timer = data_.timer;
			me.title = me.timer.name;
		} );
	};

	public submitTimer() {
		var me = this;
		if ( me.device ) {
			me.timer.device_id = me.device.id;
			delete( me.timer.scripts );
		}
		me._timersService.putTimer( me.timer )
			.subscribe(
				function( timer_: Timer ) {
					if ( !!me._timersService.returnUrl ) {
						me._router.navigateByUrl( me._timersService.returnUrl );
						delete me._timersService.returnUrl;
					} else {
						me._router.navigate( [ '/timers' ] );
					}
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	public deleteTimer() {
		var me = this;
		me._timersService.deleteTimer( me.timer )
			.subscribe(
				function( timer_: Timer ) {
					if ( !!me._timersService.returnUrl ) {
						me._router.navigateByUrl( me._timersService.returnUrl );
						delete me._timersService.returnUrl;
					} else {
						me._router.navigate( [ '/timers' ] );
					}
				},
				function( error_: string ) {
					me.error = error_;
				}
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
