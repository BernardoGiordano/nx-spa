import {Component} from '@angular/core';
import {HttpClient} from "@angular/common/http";
import {SumApiResponse} from "./app.model";

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css']
})
export class AppComponent {
  title = 'sample-angular-app';

  remoteCallResult: number | undefined = undefined;

  constructor(
    private httpClient: HttpClient
  ) {
    this.httpClient.get<SumApiResponse>(
      "http://0.0.0.0:8080/sum", {
        params: {
          n1: 3,
          n2: 4
        }
      }
    ).subscribe(response => {
      this.remoteCallResult = response.result;
    })
  }
}
