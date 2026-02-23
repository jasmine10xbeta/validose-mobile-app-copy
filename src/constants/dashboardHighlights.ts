export const dashboardHighlightFlags = {
  // Temporary global switch to force-show replace banner on dashboard.
  showReplaceMedicineForAll: true,

  // Temporary flag source for replace-medicine prompts.
  // This will be replaced by background-driven queue data.
  replaceMedicineByMedicationLabel: {
    A: true,
  } as Record<string, boolean>,
};

export function shouldShowReplaceMedicineBanner(medicationLabel: string): boolean {
  if (dashboardHighlightFlags.showReplaceMedicineForAll) return true;
  return dashboardHighlightFlags.replaceMedicineByMedicationLabel[medicationLabel] === true;
}
