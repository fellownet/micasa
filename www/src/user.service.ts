import { Injectable }  from '@angular/core';

export enum ACL {
	Viewer = 1,
	User = 2,
	Admin = 4
}

export class User {
	username: string;
	fullname: string;
	acl: number;
}

@Injectable()
export class UserService {


}
