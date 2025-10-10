import { clsx, type ClassValue } from "clsx"
import { twMerge } from "tailwind-merge"

import { buildAbsoluteUrl } from "@/lib/app-url"

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}

export function absoluteUrl(path: string) {
  return buildAbsoluteUrl(path)
}
