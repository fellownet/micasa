import { Component, OnInit }      from '@angular/core';
import { Router, ActivatedRoute } from '@angular/router';
import { Timer, TimersService }   from './timers.service';
import { Script, ScriptsService } from '../scripts/scripts.service';

@Component( {
	templateUrl: 'tpl/timer-details.html',
} )

export class TimerDetailsComponent implements OnInit {

	loading: boolean = false;
	error: String;
	timer: Timer;
	scripts: Script[] = [];

	private _editor: any;

	constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _timersService: TimersService,
		private _scriptsService: ScriptsService
	) {
	};

	ngOnInit() {
		var me = this;
		this.getScripts();
		this._route.data.subscribe( function( data_: any ) {
			me.timer = data_.timer;
		} );
	};

	submitTimer() {
		var me = this;
		me.loading = true;
		this._timersService.putTimer( me.timer )
			.subscribe(
				function( timer_: Timer ) {
					me.loading = false;
					me._router.navigate( [ '/timers' ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	deleteTimer() {
		var me = this;
		me._timersService.deleteTimer( me.timer )
			.subscribe(
				function( success_: boolean ) {
					me.loading = false;
					if ( success_ ) {
						me._router.navigate( [ '/timers' ] );
					} else {
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

	getScripts() {
		var me = this;
		this._scriptsService.getScripts()
			.subscribe(
				function( scripts_: Script[]) {
					me.scripts = scripts_;
				},
				function( error_: String ) {
					me.error = error_;
				}
			)
		;
	};

	updateSelectedScripts( id_: number, event_: any ) {
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
