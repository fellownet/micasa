import { Component, OnInit }      from '@angular/core';
import { Router }                 from '@angular/router';
import { Timer, TimersService }   from './timers.service';

declare var $: any;
declare var ace: any;

@Component( {
	templateUrl: 'tpl/timers-list.html',
} )

export class TimersListComponent implements OnInit {

	loading: Boolean = false;
	error: String;
	timers: Timer[] = [];

	constructor(
		private _router: Router,
		private _timersService: TimersService
	) {
	};


	ngOnInit() {
		this.getTimers();
	};

	getTimers() {
		var me = this;
		this._timersService.getTimers()
			.subscribe(
				function( timers_: Timer[]) {
					me.timers = timers_;
				},
				function( error_: String ) {
					me.error = error_;
				}
			)
		;
	};

	selectTimer( timer_: Timer ) {
		this._router.navigate( [ '/timers', timer_.id ] );
	};

	addTimer() {
		this._router.navigate( [ '/timers', 'add' ] );
	};

}
