import {
	Component,
	OnInit,
	Input
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';

import {
	Device,
	Link
}                  from '../devices.service';

@Component( {
	selector: 'links',
	templateUrl: 'tpl/links-list.html'
} )

export class LinksListComponent implements OnInit {

	public loading: boolean = false;
	public error: String;
	public links: Link[];

	@Input() public device: Device;
	
	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data
			.subscribe( function( data_: any ) {
				me.links = data_.links;
			} )
		;
	};

	public selectLink( link_: Link ) {
		this.loading = true;
		this._router.navigate( [ '/devices', link_.device_id, 'links', link_.id ] );
	};

	public addLink() {
		this.loading = true;
		this._router.navigate( [ '/devices', this.device.id, 'links', 'add' ] );
	};
}
