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

	public loading: boolean = false;
	public error: String;
	public timer: Timer;

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
		} );
	};

	public submitTimer() {
		var me = this;
		me.loading = true;
		if ( me.device ) {
			me.timer.device_id = me.device.id;
			delete( me.timer.scripts );
		}
		this._timersService.putTimer( me.timer )
			.subscribe(
				function( timer_: Timer ) {
					if ( me.device ) {
						me._router.navigate( [ '/devices', me.device.id, 'edit' ] );
					} else {
						me._router.navigate( [ '/timers' ] );
					}
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	public deleteTimer() {
		var me = this;
		me.loading = true;
		me._timersService.deleteTimer( me.timer )
			.subscribe(
				function( success_: boolean ) {
					if ( success_ ) {
						if ( me.device ) {
							me._router.navigate( [ '/devices', me.device.id, 'edit' ] );
						} else {
							me._router.navigate( [ '/timers' ] );
						}
					} else {
						me.loading = false;
						this.error = 'Unable to delete timer.';
					}
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	public updateSelectedScripts( id_: number, event_: any ) {
		let target: any = event.target;
		if ( target.checked ) {
			if ( this.timer.scripts.indexOf( id_ ) < 0 ) {
				this.timer.scripts.push( id_ );
			}
		} else {
			let pos: number = this.timer.scripts.indexOf( id_ );
			if ( pos >= 0 ) {
				this.timer.scripts.splice( pos, 1 );
			}
		}
	};
}
