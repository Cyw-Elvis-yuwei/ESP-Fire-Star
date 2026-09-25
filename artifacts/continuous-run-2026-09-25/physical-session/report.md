# CAN diagnostic session

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../../publication/README.md)。

- Simulated: false
- Hardware verified: **false**
- Interface: can0
- Connected: false
- Online (fresh heartbeat): false
- Log healthy: true
- Log error: 

Software protocol observations only. Virtual CAN does not verify physical CAN, STM32 firmware, wiring or the physical LED. Restart events are observations from counters and uptime; a new matching command is needed to demonstrate recovery.

## Counters

| Counter | Total |
| --- | ---: |
| commands_attempted | 581 |
| commands_failed | 0 |
| commands_rejected | 0 |
| commands_sent | 581 |
| commands_succeeded | 581 |
| commands_timeouts | 0 |
| duplicate_heartbeats | 0 |
| heartbeat_timeouts | 0 |
| ignored_frames | 0 |
| invalid_frames | 0 |
| local_echo_frames | 0 |
| log_errors | 0 |
| restart_observations | 0 |
| results_dropped | 325 |
| rx_frames | 1206 |
| stale_heartbeats | 0 |
| transport_errors | 0 |
| tx_frames | 581 |
| unmatched_responses | 0 |
| valid_heartbeats | 625 |

## Recent command results

At most 256 results retained; counters cover the whole session.

| Sequence | Operation | Success | Outcome | Latency ms |
| ---: | ---: | --- | --- | ---: |
| 326 | 2 | true | success | 3 |
| 327 | 2 | true | success | 2 |
| 328 | 2 | true | success | 3 |
| 329 | 2 | true | success | 3 |
| 330 | 2 | true | success | 2 |
| 331 | 2 | true | success | 3 |
| 332 | 2 | true | success | 4 |
| 333 | 2 | true | success | 3 |
| 334 | 2 | true | success | 3 |
| 335 | 2 | true | success | 3 |
| 336 | 2 | true | success | 3 |
| 337 | 2 | true | success | 3 |
| 338 | 2 | true | success | 3 |
| 339 | 2 | true | success | 4 |
| 340 | 2 | true | success | 3 |
| 341 | 2 | true | success | 3 |
| 342 | 2 | true | success | 3 |
| 343 | 2 | true | success | 3 |
| 344 | 2 | true | success | 3 |
| 345 | 2 | true | success | 2 |
| 346 | 2 | true | success | 2 |
| 347 | 2 | true | success | 2 |
| 348 | 2 | true | success | 3 |
| 349 | 2 | true | success | 3 |
| 350 | 2 | true | success | 3 |
| 351 | 2 | true | success | 3 |
| 352 | 2 | true | success | 3 |
| 353 | 2 | true | success | 3 |
| 354 | 2 | true | success | 4 |
| 355 | 2 | true | success | 4 |
| 356 | 2 | true | success | 3 |
| 357 | 2 | true | success | 3 |
| 358 | 2 | true | success | 3 |
| 359 | 2 | true | success | 3 |
| 360 | 2 | true | success | 3 |
| 361 | 2 | true | success | 3 |
| 362 | 2 | true | success | 3 |
| 363 | 2 | true | success | 3 |
| 364 | 2 | true | success | 3 |
| 365 | 2 | true | success | 3 |
| 366 | 2 | true | success | 3 |
| 367 | 2 | true | success | 3 |
| 368 | 2 | true | success | 3 |
| 369 | 2 | true | success | 2 |
| 370 | 2 | true | success | 3 |
| 371 | 2 | true | success | 3 |
| 372 | 2 | true | success | 3 |
| 373 | 2 | true | success | 3 |
| 374 | 2 | true | success | 3 |
| 375 | 2 | true | success | 3 |
| 376 | 2 | true | success | 3 |
| 377 | 2 | true | success | 4 |
| 378 | 2 | true | success | 3 |
| 379 | 2 | true | success | 2 |
| 380 | 2 | true | success | 2 |
| 381 | 2 | true | success | 3 |
| 382 | 2 | true | success | 3 |
| 383 | 2 | true | success | 3 |
| 384 | 2 | true | success | 3 |
| 385 | 2 | true | success | 3 |
| 386 | 2 | true | success | 3 |
| 387 | 2 | true | success | 2 |
| 388 | 2 | true | success | 2 |
| 389 | 2 | true | success | 3 |
| 390 | 2 | true | success | 3 |
| 391 | 2 | true | success | 3 |
| 392 | 2 | true | success | 3 |
| 393 | 2 | true | success | 4 |
| 394 | 2 | true | success | 3 |
| 395 | 2 | true | success | 3 |
| 396 | 2 | true | success | 3 |
| 397 | 2 | true | success | 4 |
| 398 | 2 | true | success | 3 |
| 399 | 2 | true | success | 2 |
| 400 | 2 | true | success | 3 |
| 401 | 2 | true | success | 3 |
| 402 | 2 | true | success | 4 |
| 403 | 2 | true | success | 3 |
| 404 | 2 | true | success | 3 |
| 405 | 2 | true | success | 3 |
| 406 | 2 | true | success | 4 |
| 407 | 2 | true | success | 2 |
| 408 | 2 | true | success | 2 |
| 409 | 2 | true | success | 3 |
| 410 | 2 | true | success | 3 |
| 411 | 2 | true | success | 3 |
| 412 | 2 | true | success | 4 |
| 413 | 2 | true | success | 3 |
| 414 | 2 | true | success | 3 |
| 415 | 2 | true | success | 4 |
| 416 | 2 | true | success | 3 |
| 417 | 2 | true | success | 2 |
| 418 | 2 | true | success | 3 |
| 419 | 2 | true | success | 3 |
| 420 | 2 | true | success | 2 |
| 421 | 2 | true | success | 3 |
| 422 | 2 | true | success | 3 |
| 423 | 2 | true | success | 3 |
| 424 | 2 | true | success | 3 |
| 425 | 2 | true | success | 3 |
| 426 | 2 | true | success | 3 |
| 427 | 2 | true | success | 3 |
| 428 | 2 | true | success | 3 |
| 429 | 2 | true | success | 3 |
| 430 | 2 | true | success | 3 |
| 431 | 2 | true | success | 3 |
| 432 | 2 | true | success | 3 |
| 433 | 2 | true | success | 3 |
| 434 | 2 | true | success | 3 |
| 435 | 2 | true | success | 3 |
| 436 | 2 | true | success | 3 |
| 437 | 2 | true | success | 3 |
| 438 | 2 | true | success | 2 |
| 439 | 2 | true | success | 2 |
| 440 | 2 | true | success | 3 |
| 441 | 2 | true | success | 3 |
| 442 | 2 | true | success | 3 |
| 443 | 2 | true | success | 3 |
| 444 | 2 | true | success | 3 |
| 445 | 2 | true | success | 3 |
| 446 | 2 | true | success | 3 |
| 447 | 2 | true | success | 3 |
| 448 | 2 | true | success | 2 |
| 449 | 2 | true | success | 3 |
| 450 | 2 | true | success | 3 |
| 451 | 2 | true | success | 3 |
| 452 | 2 | true | success | 2 |
| 453 | 2 | true | success | 2 |
| 454 | 2 | true | success | 2 |
| 455 | 2 | true | success | 2 |
| 456 | 2 | true | success | 3 |
| 457 | 2 | true | success | 3 |
| 458 | 2 | true | success | 2 |
| 459 | 2 | true | success | 3 |
| 460 | 2 | true | success | 3 |
| 461 | 2 | true | success | 3 |
| 462 | 2 | true | success | 3 |
| 463 | 2 | true | success | 3 |
| 464 | 2 | true | success | 3 |
| 465 | 2 | true | success | 3 |
| 466 | 2 | true | success | 3 |
| 467 | 2 | true | success | 3 |
| 468 | 2 | true | success | 3 |
| 469 | 2 | true | success | 3 |
| 470 | 2 | true | success | 3 |
| 471 | 2 | true | success | 2 |
| 472 | 2 | true | success | 3 |
| 473 | 2 | true | success | 4 |
| 474 | 2 | true | success | 3 |
| 475 | 2 | true | success | 2 |
| 476 | 2 | true | success | 3 |
| 477 | 2 | true | success | 3 |
| 478 | 2 | true | success | 3 |
| 479 | 2 | true | success | 3 |
| 480 | 2 | true | success | 3 |
| 481 | 2 | true | success | 3 |
| 482 | 2 | true | success | 3 |
| 483 | 2 | true | success | 2 |
| 484 | 2 | true | success | 3 |
| 485 | 2 | true | success | 3 |
| 486 | 2 | true | success | 3 |
| 487 | 2 | true | success | 3 |
| 488 | 2 | true | success | 3 |
| 489 | 2 | true | success | 3 |
| 490 | 2 | true | success | 3 |
| 491 | 2 | true | success | 2 |
| 492 | 2 | true | success | 3 |
| 493 | 2 | true | success | 3 |
| 494 | 2 | true | success | 3 |
| 495 | 2 | true | success | 3 |
| 496 | 2 | true | success | 2 |
| 497 | 2 | true | success | 3 |
| 498 | 2 | true | success | 3 |
| 499 | 2 | true | success | 3 |
| 500 | 2 | true | success | 2 |
| 501 | 2 | true | success | 3 |
| 502 | 2 | true | success | 3 |
| 503 | 2 | true | success | 3 |
| 504 | 2 | true | success | 3 |
| 505 | 2 | true | success | 3 |
| 506 | 2 | true | success | 2 |
| 507 | 2 | true | success | 3 |
| 508 | 2 | true | success | 2 |
| 509 | 2 | true | success | 2 |
| 510 | 2 | true | success | 3 |
| 511 | 2 | true | success | 3 |
| 512 | 2 | true | success | 3 |
| 513 | 2 | true | success | 2 |
| 514 | 2 | true | success | 3 |
| 515 | 2 | true | success | 3 |
| 516 | 2 | true | success | 3 |
| 517 | 2 | true | success | 3 |
| 518 | 2 | true | success | 13 |
| 519 | 2 | true | success | 2 |
| 520 | 2 | true | success | 3 |
| 521 | 2 | true | success | 3 |
| 522 | 2 | true | success | 3 |
| 523 | 2 | true | success | 2 |
| 524 | 2 | true | success | 3 |
| 525 | 2 | true | success | 3 |
| 526 | 2 | true | success | 3 |
| 527 | 2 | true | success | 3 |
| 528 | 2 | true | success | 3 |
| 529 | 2 | true | success | 3 |
| 530 | 2 | true | success | 3 |
| 531 | 2 | true | success | 3 |
| 532 | 2 | true | success | 4 |
| 533 | 2 | true | success | 3 |
| 534 | 2 | true | success | 3 |
| 535 | 2 | true | success | 4 |
| 536 | 2 | true | success | 3 |
| 537 | 2 | true | success | 2 |
| 538 | 2 | true | success | 3 |
| 539 | 2 | true | success | 2 |
| 540 | 2 | true | success | 2 |
| 541 | 2 | true | success | 3 |
| 542 | 2 | true | success | 3 |
| 543 | 2 | true | success | 3 |
| 544 | 2 | true | success | 3 |
| 545 | 2 | true | success | 3 |
| 546 | 2 | true | success | 2 |
| 547 | 2 | true | success | 3 |
| 548 | 2 | true | success | 3 |
| 549 | 2 | true | success | 3 |
| 550 | 2 | true | success | 2 |
| 551 | 2 | true | success | 3 |
| 552 | 2 | true | success | 2 |
| 553 | 2 | true | success | 3 |
| 554 | 2 | true | success | 2 |
| 555 | 2 | true | success | 2 |
| 556 | 2 | true | success | 2 |
| 557 | 2 | true | success | 3 |
| 558 | 2 | true | success | 2 |
| 559 | 2 | true | success | 2 |
| 560 | 2 | true | success | 3 |
| 561 | 2 | true | success | 3 |
| 562 | 2 | true | success | 3 |
| 563 | 2 | true | success | 3 |
| 564 | 2 | true | success | 4 |
| 565 | 2 | true | success | 3 |
| 566 | 2 | true | success | 3 |
| 567 | 2 | true | success | 3 |
| 568 | 2 | true | success | 3 |
| 569 | 2 | true | success | 2 |
| 570 | 2 | true | success | 3 |
| 571 | 2 | true | success | 3 |
| 572 | 2 | true | success | 3 |
| 573 | 2 | true | success | 2 |
| 574 | 2 | true | success | 3 |
| 575 | 2 | true | success | 3 |
| 576 | 2 | true | success | 3 |
| 577 | 2 | true | success | 4 |
| 578 | 2 | true | success | 4 |
| 579 | 2 | true | success | 2 |
| 580 | 2 | true | success | 3 |
| 581 | 2 | true | success | 2 |
