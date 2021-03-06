import {
	Component,
	OnInit,
	OnDestroy,
	Input,
	ViewChild
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';
import {
	Timer,
	TimersService
}                  from './timers.service';
import { Device }  from '../devices/devices.service';
import {
	GridPagingComponent
}                  from '../grid/paging.component'

@Component( {
	selector: 'timers',
	templateUrl: 'tpl/timers-list.html',
} )

export class TimersListComponent implements OnInit, OnDestroy {

	public timers: Timer[];
	public startPage: number = 1;

	@Input() public device?: Device; // gets set when used from the device edit component

	@ViewChild(GridPagingComponent) private _paging: GridPagingComponent;

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
					this.timers = data_.timers;
					if ( !! this.device ) {
						this.startPage = this._timersService.lastPage[this.device.id] || 1;
					} else {
						this.startPage = this._timersService.lastPage['global'] || 1;
					}
				}
			)
		;
	};

	public ngOnDestroy() {
		if ( this._paging ) {
			if ( !! this.device ) {
				this._timersService.lastPage[this.device.id] = this._paging.getActivePage();
			} else {
				this._timersService.lastPage['global'] = this._paging.getActivePage();
			}
		}
	};

	public selectTimer( timer_: Timer ) {
		this._timersService.returnUrl = this._router.url;
		if ( !! this.device ) {
			this._router.navigate( [ '/timers', timer_.id, 'device', this.device.id ] );
		} else {
			this._router.navigate( [ '/timers', timer_.id ] );
		}

	};

	public addTimer() {
		this._timersService.returnUrl = this._router.url;
		if ( this.device ) {
			this._router.navigate( [ '/timers', 'add', 'device', this.device.id ] );
		} else {
			this._router.navigate( [ '/timers', 'add' ] );
		}
	};

}
