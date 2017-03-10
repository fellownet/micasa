import {
	Pipe,
	PipeTransform
}                 from '@angular/core';

@Pipe( {
	name: 'truncate'
} )

export class TruncatePipe implements PipeTransform {

	public transform( value_: string, limit_: number = 10, ellipsis_: string = '...' ): string {
		return value_.length > limit_ ? value_.substring( 0, limit_ ) + ellipsis_ : value_;
	}
}
