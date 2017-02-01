import {
	Component,
	OnInit,
	Input
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';

import { Timer }   from './timers.service';
import { Device }  from '../devices/devices.service';

@Component( {
	selector: 'timers',
	templateUrl: 'tpl/timers-list.html',
} )

export class TimersListComponent implements OnInit {

	public loading: boolean = false;
	public error: String;
	public timers: Timer[];

	@Input() public device?: Device; // gets set when used from the device edit component

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.timers = data_.timers;
		} );
	};

	public selectTimer( timer_: Timer ) {
		this.loading = true;
		if ( this.device ) {
			this._router.navigate( [ '/devices', this.device.id, 'timers', timer_.id ] );
		} else {
			this._router.navigate( [ '/timers', timer_.id ] );
		}
	};

	public addTimer() {
		this.loading = true;
		if ( this.device ) {
			this._router.navigate( [ '/devices', this.device.id, 'timers', 'add' ] );
		} else {
			this._router.navigate( [ '/timers', 'add' ] );
		}
	};
}
