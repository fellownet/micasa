import {
	Component,
	Input,
	OnInit,
	OnDestroy
}                        from '@angular/core';
import { Subject }       from 'rxjs/Subject';
import { GridDirective } from "./grid.directive";

@Component( {
	selector: 'grid-search',
	template: `
		<div class="input-group">
			<div class="input-group-addon"><i class="fa fa-search"></i></div>
			<input type="search" class="form-control" placeholder="Search" autocorrect="off" autocapitalize="none" (keyup)="updateSearch( $event.target.value )">
		</div>
		`
} )

export class GridSearchComponent implements OnInit, OnDestroy {

	private _change: Subject<string> = new Subject();
	private _active: boolean = true;

	@Input( "grid" ) private _grid: GridDirective;

	public ngOnInit() {
		this._change
			.takeWhile( () => this._active )
			.debounceTime( 350 )
			.subscribe( value_ => {
				this._grid.setSearch( value_ );
			} )
		;
	};

	public ngOnDestroy() {
		this._active = false;
	};

	public updateSearch( value_: string ) {
		this._change.next( value_ );
	};

}
